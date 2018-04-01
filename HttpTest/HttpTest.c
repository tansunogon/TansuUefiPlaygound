#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/Http.h>
#include <Protocol/PxeBaseCode.h>

VOID
WaitForKey(VOID)
{
  UINTN Index;
  gST->ConIn->Reset(gST->ConIn, FALSE);
  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
}

VOID
EFIAPI
RequestCallBack (
  IN EFI_EVENT Event,
  IN VOID *Context
  )
{
  Print(L"RequestCallBack called");
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  EFI_SERVICE_BINDING_PROTOCOL *HttpSb;
  Status = gBS->LocateProtocol(
    &gEfiHttpServiceBindingProtocolGuid,
    NULL,
    (VOID **)&HttpSb);
  if (EFI_ERROR(Status)) {
    Print(L"First LocateProtocol HttpSb %r\n", Status);
  }

  UINTN NoHandles;
  EFI_HANDLE *Handles;
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiPxeBaseCodeProtocolGuid,
    NULL,
    &NoHandles,
    &Handles);
  Print(L"LocateHandleBuffer PxeBc Found %lu\n", NoHandles);

  EFI_HANDLE NetworkHandle = Handles[0];
  EFI_PXE_BASE_CODE_PROTOCOL *PxeBc;
  Status = gBS->HandleProtocol(
    NetworkHandle,
    &gEfiPxeBaseCodeProtocolGuid,
    (VOID **)&PxeBc);
  if (EFI_ERROR(Status)) {
    Print(L"HandleProtocol PxeBc %r\n", Status);
  }
  if (!PxeBc->Mode->Started) {
    Print(L"PxeBc Not Started\n");
  }
  if (!PxeBc->Mode->DhcpAckReceived) {
    Print(L"PxeBc Not DhcpAckReceived\n");
  }
  EFI_IP_ADDRESS ServerIp;
  gBS->CopyMem(&ServerIp, PxeBc->Mode->DhcpAck.Dhcpv4.BootpSiAddr, 4);
  VOID *Buffer;
  UINTN BufferSize = 100 * 1024;
  Status = gBS->AllocatePool(
    EfiLoaderData,
    BufferSize,
    &Buffer);
  if (EFI_ERROR(Status)) {
    Print(L"Allocate Mtftp Buffer %r", Status);
  }
  Status = PxeBc->Mtftp(
    PxeBc,
    EFI_PXE_BASE_CODE_TFTP_READ_FILE,
    Buffer,
    /*Overrite*/FALSE,
    &BufferSize,
    /*BlockSIze*/NULL,
    &ServerIp,
    (UINT8 *)"HttpDxe.efi",
    /*MtfpInfo*/NULL,
    /*DontUseBuffer*/FALSE);
  if (EFI_ERROR(Status)) {
    Print(L"Mtftp %r\n", Status);
  }
  EFI_DEVICE_PATH *NetworkDevPath;
  Status = gBS->HandleProtocol(
    NetworkHandle,
    &gEfiDevicePathProtocolGuid,
    (VOID **)&NetworkDevPath);
  if (EFI_ERROR(Status)) {
    Print(L"HandleProtocol NetworkDevPath %r\n", Status);
  }
  EFI_HANDLE HttpDriverHandle;
  Status = gBS->LoadImage(
    /*BootPolicy*/TRUE,
    ImageHandle,
    NetworkDevPath,
    Buffer,
    BufferSize,
    &HttpDriverHandle);
  if (EFI_ERROR(Status)) {
    Print(L"LoadImage HttpDriver %r\n", Status);
  }
  UINTN ExitDataSize;
  CHAR16 *ExitData;
  Status = gBS->StartImage(
    HttpDriverHandle,
    &ExitDataSize,
    &ExitData);
  if (EFI_ERROR(Status)) {
    Print(L"StartImage HttpDriver %r\n", Status);
  }

  Status = gBS->LocateProtocol(
    &gEfiHttpServiceBindingProtocolGuid,
    NULL,
    (VOID **)&HttpSb);
  if (EFI_ERROR(Status)) {
    Print(L"Second LocateProtocol HttpSb %r\n", Status);
    // Not Found
  }

  EFI_DRIVER_BINDING_PROTOCOL *HttpDriverBinding;
  Status = gBS->HandleProtocol(
    HttpDriverHandle,
    &gEfiDriverBindingProtocolGuid,
    (VOID **)&HttpDriverBinding);
  if (EFI_ERROR(Status)) {
    Print(L"HandleProtocol HttpDriverBinding %r\n", Status);
  }

  UINTN NoTcpSbHandles;
  EFI_HANDLE *TcpSbHandles;
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiTcp4ServiceBindingProtocolGuid,
    NULL,
    &NoTcpSbHandles,
    &TcpSbHandles);
  Print(L"NoTcpSbHandles %lu\n", NoTcpSbHandles);

  Status = HttpDriverBinding->Start(
    HttpDriverBinding,
    TcpSbHandles[0],
    NULL);
  if (EFI_ERROR(Status)) {
    Print(L"HttpDriverBinding Start %r\n", Status);
  }

  Status = gBS->LocateProtocol(
    &gEfiHttpServiceBindingProtocolGuid,
    NULL,
    (VOID **)&HttpSb);
  if (EFI_ERROR(Status)) {
    Print(L"Third LocateProtocol HttpSb %r\n", Status);
    // EFI_SUCCESS
  }

  EFI_HANDLE HttpHandle = NULL;
  Status = HttpSb->CreateChild(HttpSb, &HttpHandle);
  if (EFI_ERROR(Status)) {
    Print(L"HttpSb CreateChild %r\n", Status);
  }

  EFI_HTTP_PROTOCOL *Http;
  Status = gBS->HandleProtocol(
    HttpHandle,
    &gEfiHttpProtocolGuid,
    (VOID **)&Http);
  if (EFI_ERROR(Status)) {
    Print(L"HandleProtocol Http %r\n", Status);
  }

  EFI_HTTPv4_ACCESS_POINT AccessPoint;
  AccessPoint.UseDefaultAddress = FALSE;
  AccessPoint.LocalAddress = PxeBc->Mode->StationIp.v4;
  AccessPoint.LocalSubnet = PxeBc->Mode->SubnetMask.v4;
  AccessPoint.LocalPort = 11223;

  EFI_HTTP_CONFIG_DATA Config;
  Config.HttpVersion = HttpVersion11;
  Config.TimeOutMillisec = 1000;
  Config.LocalAddressIsIPv6 = FALSE;
  Config.AccessPoint.IPv4Node = &AccessPoint;

  Status = Http->Configure(Http, &Config);
  if (EFI_ERROR(Status)) {
    Print(L"Configure %r\n", Status);
  }

  EFI_HTTP_REQUEST_DATA RequestData;
  RequestData.Method = HttpMethodGet;
  // www.uefi.org
  RequestData.Url = L"http://54.244.19.255/sites/default/files/uefi_logo_red_web.jpg";

  EFI_HTTP_HEADER RequestHeader;
  RequestHeader.FieldName = "Host";
  RequestHeader.FieldValue = "54.244.19.255";

  EFI_HTTP_MESSAGE RequestMessage;
  RequestMessage.Data.Request = &RequestData;
  RequestMessage.HeaderCount = 1;
  RequestMessage.Headers = &RequestHeader;
  RequestMessage.BodyLength = 0;
  RequestMessage.Body = NULL;

  EFI_HTTP_TOKEN RequestToken;
  Status = gBS->CreateEvent(
    EVT_NOTIFY_SIGNAL,
    TPL_CALLBACK,
    RequestCallBack,
    NULL,
    &RequestToken.Event);
  if (EFI_ERROR(Status)) {
    Print(L"CraeteEvent %r\n", Status);
  }
  RequestToken.Status = EFI_SUCCESS;
  RequestToken.Message = &RequestMessage;

  Status = Http->Request(Http, &RequestToken);
  if (EFI_ERROR(Status)) {
    Print(L"Request %r\n", Status);
    // returned Access Denied
    // LOG: EfiHttpRequest: HTTP is disabled. (HttpImpl.c)
    //
    // gEfiNetworkPkgTokenSpaceGuid.PcdAllowHttpConnections => TRUE
    // build again
    //
    // returned Unsupported
    // (because there is no DNS protocol)
    //
    // URL (www.uefi.org) => IP (54.244.19.255)
    //
    // returned Time out
    // LOG: Tcp4 Connection fail - 12
  }

  WaitForKey();
  gST->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

  return EFI_SUCCESS;
}
