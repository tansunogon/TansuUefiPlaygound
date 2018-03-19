#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

VOID
WaitKey(VOID)
{
  UINTN Index;
  gST->ConIn->Reset(gST->ConIn, FALSE);
  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
}

VOID
WaitKeyAndReset (VOID)
{
  Print(L"\nPress any key to reset");
  WaitKey();
  gST->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  Print(L"GopTest\n");

  UINTN NoHandles;
  EFI_HANDLE *Handles;
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiGraphicsOutputProtocolGuid,
    NULL,
    &NoHandles,
    &Handles
    );
  if (EFI_ERROR(Status)) {
    Print(L"HandleBuffer %r\n", Status);
    WaitKeyAndReset();
  }

  Print(L"NumOfHandle %lu\n", NoHandles);

  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  Status = gBS->HandleProtocol(
    Handles[0],
    &gEfiGraphicsOutputProtocolGuid,
    (VOID**)&Gop
    );
  if (EFI_ERROR(Status)) {
    Print(L"HandleProtocol %r\n", Status);
    WaitKeyAndReset();
  }

  Print(
    L"%u %u\n",
    Gop->Mode->Info->HorizontalResolution,
    Gop->Mode->Info->VerticalResolution
    );

  Print(
    L"Address %lx\n",
    Gop->Mode->FrameBufferBase
    );

  Print(
    L"PixelsPerScanLine %lx\n",
    Gop->Mode->Info->PixelsPerScanLine
    );

  Print(
    L"PixelFormat %u\n",
    Gop->Mode->Info->PixelFormat
    );

#define TEST_MODE_A
#if defined(TEST_MODE_A) || defined(TEST_MODE_B)
  UINT32 Pixels = Gop->Mode->Info->HorizontalResolution * Gop->Mode->Info->VerticalResolution;
  UINTN BufferSize = sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * Pixels;
#else
  UINTN BufferSize = Gop->Mode->FrameBufferSize;
#endif
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer;
  Status = gBS->AllocatePool(EfiLoaderData, BufferSize, (VOID**)&Buffer);
  if (EFI_ERROR(Status)) {
    Print(L"AllocatePool %r\n", Status);
    WaitKeyAndReset();
  }

  Print(L"BufferAddress %lx\n", Buffer);
  WaitKey();

  gBS->SetMem(Buffer, BufferSize, 0);

#if 0
  UINTN MemoryMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
  gBS->GetMemoryMap(
    &MemoryMapSize,
    MemoryMap,
    NULL,
    NULL,
    NULL);
  gBS->AllocatePool(EfiLoaderData, MemoryMapSize, (VOID **)&MemoryMap);
  UINTN MapKey;
  UINTN DescriptorSize;
  UINT32 DescriptorVersion;
  Status = gBS->GetMemoryMap(
    &MemoryMapSize,
    MemoryMap,
    &MapKey,
    &DescriptorSize,
    &DescriptorVersion);
  if (EFI_ERROR(Status)) {
    Print(L"GetMemoryMap %r\n", Status);
  }
  gBS->ExitBootServices(ImageHandle, MapKey);
#endif

  while (gImageHandle) {
    for (UINT32 i = 0; i <= 0xff; ++i) {
      UINT8 *BufferRaw = (UINT8 *)Buffer;
      for (UINTN j = 0; j <= BufferSize; ++j) {
        BufferRaw[j] = (UINT8)i;
      }
      // gBS->SetMem(Buffer, BufferSize, (UINT8)i);
#if defined(TEST_MODE_A)
      // takes 65.9 seconds for 10 loops
      // => 256 * 10 / 65.9 = 38.8 fps
      Gop->Blt(
        Gop,
        Buffer,
        EfiBltBufferToVideo,
        0, 0,
        0, 0,
        Gop->Mode->Info->HorizontalResolution, Gop->Mode->Info->VerticalResolution,
        0
        );
#elif defined(TEST_MODE_B)
      // takes 65.9 seconds for 10 loops
      // => 256 * 10 / 65.9 = 38.8 fps
      Gop->Blt(
        Gop,
        Buffer,
        EfiBltVideoFill,
        0, 0,
        0, 0,
        Gop->Mode->Info->HorizontalResolution, Gop->Mode->Info->VerticalResolution,
        0
      );
#elif defined(TEST_MODE_C)
      // takes 75.0 seconds for 5 loops
      // => 256 * 5 / 75.0 = 17.1 fps
      gBS->CopyMem((VOID*)Gop->Mode->FrameBufferBase, Buffer, Gop->Mode->FrameBufferSize);
#elif defined(TEST_MODE_D)
      // takes 75.1 seconds for 5 loops
      // => 256 * 5 / 75.1 = 17.0 fps
      UINT64 *CastedFrameBuf = (UINT64 *)Gop->Mode->FrameBufferBase;
      UINT64 *CastedBuf = (UINT64 *)Buffer;
      for (int ci = 0; ci < (int)Gop->Mode->FrameBufferSize / 4; ++ci) {
        CastedFrameBuf[ci] = CastedBuf[ci];
      }
#elif defined(TEST_MODE_E)
      // takes 29.9 seconds for 1 loop
      // => 256 * 1 / 29.9 = 8.6 fps
      UINT32 *CastedFrameBuf = (UINT32 *)Gop->Mode->FrameBufferBase;
      UINT32 *CastedBuf = (UINT32 *)Buffer;
      for (int ci = 0; ci < (int)Gop->Mode->FrameBufferSize / 4; ++ci) {
        CastedFrameBuf[ci] = CastedBuf[ci];
      }
#endif
    }
  }

  WaitKeyAndReset();
  return EFI_SUCCESS;
}

