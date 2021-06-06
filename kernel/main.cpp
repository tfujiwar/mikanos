#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>

#include "asmfunc.h"
#include "console.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "mouse.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "queue.hpp"
#include "segment.hpp"
#include "timer.hpp"

#include "usb/classdriver/mouse.hpp"
#include "usb/device.hpp"
#include "usb/memory.hpp"
#include "usb/xhci/trb.hpp"
#include "usb/xhci/xhci.hpp"

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}

char console_buf[sizeof(Console)];
Console *console;

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter *pixel_writer;

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager *memory_manager;

usb::xhci::Controller *xhc;

struct Message {
  enum Type {
    kInterruptXHCI,
  } type;
};

ArrayQueue<Message> *main_queue;

unsigned int mouse_layer_id;
Vector2D<int> mouse_pos;
Vector2D<int> screen_size;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
  auto newpos = mouse_pos + Vector2D<int>{displacement_x, displacement_y};

  newpos.x = std::min(newpos.x, screen_size.x - 1);
  newpos.y = std::min(newpos.y, screen_size.y - 1);

  mouse_pos.x = std::max(newpos.x, 0);
  mouse_pos.y = std::max(newpos.y, 0);

  layer_manager->Move(mouse_layer_id, mouse_pos);
}

__attribute__((interrupt))
void IntHandlerXHCI(InterruptFrame *frame) {
  main_queue->Push(Message{Message::kInterruptXHCI});
  NotifyEndOfInterrupt();
}

int printk(const char *format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig &frame_buffer_config_ref,
    const MemoryMap &memory_map_ref) {

  FrameBufferConfig frame_buffer_config{frame_buffer_config_ref};
  MemoryMap memory_map{memory_map_ref};

  // Setup Segmentation
  const uint16_t kernel_cs = 1 << 3;
  const uint16_t kernel_ss = 2 << 3;
  SetupSegments();
  SetDSAll(0);
  SetCSSS(kernel_cs, kernel_ss);
  SetupIdentityPageTable();

  // Setup Memory Manager
  ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

  const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
  uintptr_t available_end = 0;

  for (uintptr_t iter = memory_map_base;
       iter < memory_map_base + memory_map.map_size;
       iter += memory_map.descriptor_size) {

    auto desc = reinterpret_cast<const MemoryDescriptor *>(iter);
    if (available_end < desc->physical_start) {
      memory_manager->MarkAllocated(
          FrameID{available_end / kBytesPerFrame},
          (desc->physical_start - available_end) / kBytesPerFrame);
    }

    const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
    if (IsAvailable(static_cast<MemoryType>(desc->type))) {
      available_end = physical_end;
    } else {
      memory_manager->MarkAllocated(
          FrameID{desc->physical_start / kBytesPerFrame},
          desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
    }
  }

  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

  if (auto err = InitializeHeap(*memory_manager)) {
    Log(kError, "failed to allocate pages: %s at %s:%d\n", err.Name(), err.File(), err.Line());
    exit(1);
  }

  // Draw desktop
  switch (frame_buffer_config.pixel_format) {
  case kPixelRGBResv8BitPerColor:
    pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter(frame_buffer_config);
    break;
  case kPixelBGRResv8BitPerColor:
    pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter(frame_buffer_config);
    break;
  }

  const int kFrameWidth = frame_buffer_config.horizontal_resolution;
  const int kFrameHeight = frame_buffer_config.vertical_resolution;
  const PixelColor kDesktopBGColor = {45, 118, 237};
  const PixelColor kDesktopFGColor = {255, 255, 255};

  FillRectangle(*pixel_writer, {0, 0}, {kFrameWidth, kFrameHeight - 50}, kDesktopBGColor);
  FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth, 50}, {1, 8, 17});
  FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50}, {80, 80, 80});
  DrawRectangle(*pixel_writer, {10, kFrameHeight - 40}, {30, 30}, {160, 160, 160});

  SetLogLevel(kWarn);
  InitializeLAPICTimer();

  console = new(console_buf) Console(kDesktopFGColor, kDesktopBGColor);
  console->SetWriter(pixel_writer);

  std::array<Message, 32> main_queue_data;
  ArrayQueue<Message> main_queue{main_queue_data};
  ::main_queue = &main_queue;

  // Print Memory Map
  const std::array<MemoryType, 3> available_memory_types{
    MemoryType::kEfiBootServicesCode,
    MemoryType::kEfiBootServicesData,
    MemoryType::kEfiConventionalMemory,
  };

  Log(kDebug, "memory_map: %p\n", &memory_map);
  for (uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map.buffer);
       iter < reinterpret_cast<uintptr_t>(memory_map.buffer) + memory_map.map_size;
       iter += memory_map.descriptor_size) {
    auto desc = reinterpret_cast<MemoryDescriptor *>(iter);

    for (int i = 0; i < available_memory_types.size(); ++i) {
      if (desc->type == available_memory_types[i]) {
        Log(kDebug, "type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
               desc->type,
               desc->physical_start,
               desc->physical_start + desc->number_of_pages * 4096 - 1,
               desc->number_of_pages,
               desc->attribute);
      }
    }
  }

  // Scan PCI devices
  auto err = pci::ScanAllBus();
  Log(kDebug, "Scan All Bus: %s\n", err.Name());

  for (int i = 0; i < pci::num_device; ++i) {
    const auto &dev = pci::devices[i];
    auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
    auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
    Log(kDebug,
      "%d.%d.%d: vend %04x, class %08x, head %02x\n",
      dev.bus, dev.device, dev.function, vendor_id, class_code, dev.header_type
    );
  }

  // Find USB xHCI
  pci::Device *xhc_dev = nullptr;
  for (int i = 0; i < pci::num_device; ++i) {
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
      xhc_dev = &pci::devices[i];
      if (pci::ReadVendorId(*xhc_dev) == 0x8086) {
        break;
      }
    }
  }
  if (xhc_dev) {
    Log(
      kInfo, "xHC has been found: %d.%d.%d\n",
      xhc_dev->bus, xhc_dev->device, xhc_dev->function
    );
  }

  // Set mouse interrupt
  const uint16_t cs = GetCS();
  SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI), cs);

  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

  const uint8_t bsp_local_apic_id = *reinterpret_cast<const uint32_t *>(0xfee00020) >> 24;
  pci::ConfigureMSIFixedDestination(
      *xhc_dev, bsp_local_apic_id,
      pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
      InterruptVector::kXHCI, 0);

  // Initialize xHC
  const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
  Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());

  const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
  Log(kDebug, "xHC mmio_base: %08lx\n", xhc_mmio_base);

  usb::xhci::Controller xhc{xhc_mmio_base};
  {
    Error err = xhc.Initialize();
    Log(kDebug, "xhc.Initialize: %s\n", err.Name());
  }
  Log(kInfo, "xHC starting\n");
  xhc.Run();

  ::xhc = &xhc;

  // Connect to USB port
  usb::HIDMouseDriver::default_observer = MouseObserver;

  for (int i = 1; i <= xhc.MaxPorts(); ++i) {
    auto port = xhc.PortAt(i);
    Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

    if (port.IsConnected()) {
      if (auto err = ConfigurePort(xhc, port)) {
        Log(kError, "failed to configure port: %s at %s:%d\n", err.Name(), err.File(), err.Line());
        continue;
      }
    }
  }

  // Setup Layers
  auto bgwindow = std::make_shared<Window>(kFrameWidth, kFrameHeight, frame_buffer_config.pixel_format);
  auto bgwriter = bgwindow->Writer();

  FillRectangle(*bgwriter, {0, 0}, {kFrameWidth, kFrameHeight - 50}, kDesktopBGColor);
  FillRectangle(*bgwriter, {0, kFrameHeight - 50}, {kFrameWidth, 50}, {1, 8, 17});
  FillRectangle(*bgwriter, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50}, {80, 80, 80});
  DrawRectangle(*bgwriter, {10, kFrameHeight - 40}, {30, 30}, {160, 160, 160});

  auto mouse_window = std::make_shared<Window>(kMouseCursorWidth, kMouseCursorHeight, frame_buffer_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

  auto main_window = std::make_shared<Window>(160, 52, frame_buffer_config.pixel_format);
  DrawWindow(*main_window->Writer(), "Hello Window");

  auto console_window = std::make_shared<Window>(
    Console::kColumns * 8, Console::kRows * 16, frame_buffer_config.pixel_format);

  console->SetWindow(console_window);

  FrameBuffer screen;
  if (auto err = screen.Initialize(frame_buffer_config)) {
    Log(kError, "failed to initialize frame buffer: %s at %s:%d\n", err.Name(), err.File(), err.Line());
  }

  screen_size.x = screen.Writer().Width();
  screen_size.y = screen.Writer().Height();

  layer_manager = new LayerManager;
  layer_manager->SetWriter(&screen);

  auto bg_layer_id = layer_manager->NewLayer()
    .SetWindow(bgwindow)
    .Move({0, 0})
    .ID();

  auto main_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .Move({200, 100})
    .ID();

  mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .Move({200, 200})
    .ID();

  mouse_pos = {200, 200};

  console->SetLayerID(layer_manager->NewLayer()
    .SetWindow(console_window)
    .Move({0, 0})
    .ID());

  layer_manager->UpDown(bg_layer_id, 0);
  layer_manager->UpDown(console->LayerID(), 1);
  layer_manager->UpDown(main_layer_id, 2);
  layer_manager->UpDown(mouse_layer_id, 3);
  layer_manager->Draw({{0, 0}, screen_size});

  // Event loop
  char str[128];
  unsigned int count = 0;

  while (1) {
    ++count;
    sprintf(str, "%010u", count);
    FillRectangle(*main_window->Writer(), {24, 28}, {80, 16}, {0xc6, 0xc6, 0xc6});
    WriteString(*main_window->Writer(), {24, 28}, str, {0, 0, 0});
    layer_manager->Draw(main_layer_id);

    __asm__("cli");
    if (main_queue.Count() == 0) {
      // __asm__("sti\n\thlt");
      __asm__("sti");
      continue;
    }

    Message msg = main_queue.Front();
    main_queue.Pop();
    __asm__("sti");

    switch (msg.type) {
    case Message::kInterruptXHCI:
      while (xhc.PrimaryEventRing()->HasFront()) {
        if (auto err = ProcessEvent(xhc)) {
          Log(kError, "Error while ProcessEvent: %s at %s:%d\n", err.Name(), err.File(), err.Line());
        }
      }
      break;
    default:
      Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }
}
