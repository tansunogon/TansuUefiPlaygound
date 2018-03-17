#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include "PxeBcImpl.h"

extern EFI_DRIVER_BINDING_PROTOCOL gPxeBcDriverBinding;

VOID
WaitKeyAndReset (VOID)
{
  UINTN Index;
  Print(L"\nPress any key to reset");
  gST->ConIn->Reset(gST->ConIn, FALSE);
  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
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

  Print(L"PxeStarter\n");

  UINTN NumOfNetworkHandles;
  EFI_HANDLE *NetworkHandles;
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiDhcp4ServiceBindingProtocolGuid,
    NULL,
    &NumOfNetworkHandles,
    &NetworkHandles
    );
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer %r\n", Status);
    WaitKeyAndReset();
  }

  gPxeBcDriverBinding.DriverBindingHandle = gImageHandle;
  gPxeBcDriverBinding.ImageHandle = gImageHandle;

  Status = gPxeBcDriverBinding.Supported(
    &gPxeBcDriverBinding,
    NetworkHandles[0],
    NULL
    );
  if (EFI_ERROR(Status)) {
    Print(L"DriverBinding Sported %r\n", Status);
    WaitKeyAndReset();
  }

  Status = gPxeBcDriverBinding.Start(
    &gPxeBcDriverBinding,
    NetworkHandles[0],
    NULL
    );
  if (EFI_ERROR(Status)) {
    Print(L"DriverBinding Start %r\n", Status);
    WaitKeyAndReset();
  }

  EFI_PXE_BASE_CODE_PROTOCOL *Pxebc;
  Status = gBS->HandleProtocol(
    NetworkHandles[0],
    &gEfiPxeBaseCodeProtocolGuid,
    (VOID**)&Pxebc
    );
  if (EFI_ERROR(Status)) {
    Print(L"Locate Pxebc %r\n", Status);
    WaitKeyAndReset();
  }

  Status = Pxebc->Start(Pxebc, FALSE);
  if (EFI_ERROR(Status)) {
    Print(L"Pxebc Start %r\n", Status);
    WaitKeyAndReset();
  }

  EFI_LOAD_FILE_PROTOCOL *LoadFile;
  Status = gBS->HandleProtocol(
    NetworkHandles[0],
    &gEfiLoadFileProtocolGuid,
    (VOID**)&LoadFile
    );
  if (EFI_ERROR(Status)) {
    Print(L"Locate LoadFile %r\n", Status);
    WaitKeyAndReset();
  }

  UINTN BufferSize = SIZE_1MB * 10;
  VOID *Buffer;
  Status = gBS->AllocatePool(EfiLoaderData, BufferSize, &Buffer);
  if (EFI_ERROR(Status)) {
    Print(L"Allocate buf %r\n", Status);
    WaitKeyAndReset();
  }

  Status = LoadFile->LoadFile(LoadFile, NULL, TRUE, &BufferSize, Buffer);
  if (EFI_ERROR(Status)) {
    Print(L"LoadFile %r\n", Status);
    WaitKeyAndReset();
  }

  Print(L"end\n");
  WaitKeyAndReset();
  return EFI_SUCCESS;
}

