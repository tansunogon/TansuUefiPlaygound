#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Protocol/Tcp4.h>
#include <Protocol/ServiceBinding.h>
#include <Protocol/PxeBaseCode.h>

VOID
WaitForKey(VOID)
{
  UINTN Index;
  gST->ConIn->Reset(gST->ConIn, FALSE);
  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
}

BOOLEAN
OutputHttp(VOID *Data, UINT32 DataLength, BOOLEAN IsHeader)
{
  CHAR8 *Buf = (CHAR8 *)Data;
  for (UINT32 i = 0; i < DataLength; ++i) {
    CHAR16 Str[3];
    if (!IsHeader && Buf[i] == '\n') {
      Str[0] = '\r';
      Str[1] = '\n';
      Str[2] = '\0';
    } else {
      Str[0] = Buf[i];
      Str[1] = '\0';
    }
    if (i >= 3
      && Buf[i - 3] == '\r'
      && Buf[i - 2] == '\n'
      && Buf[i - 1] == '\r'
      && Buf[i] == '\n') {
      IsHeader = FALSE;
    }
    gST->ConOut->OutputString(gST->ConOut, Str);
  }
  return IsHeader;
}

BOOLEAN HasConnected = FALSE;
BOOLEAN HasTransmitted = FALSE;
BOOLEAN HasReceived = FALSE;

VOID
EFIAPI
ConnectionCallBack (
  IN EFI_EVENT Event,
  IN VOID *Context
  )
{
  Print(L"Connection CallBack\n");
  HasConnected = TRUE;
}

VOID
EFIAPI
TransmitCallBack (
  IN EFI_EVENT Event,
  IN VOID *Context
  )
{
  Print(L"Transmit CallBack\n");
  HasTransmitted = TRUE;
}

VOID
EFIAPI
ReceiveCallBack (
  IN EFI_EVENT Event,
  IN VOID *Context
)
{
  Print(L"Receive CallBack\n");
  HasReceived = TRUE;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  EFI_STATUS Status;

  Print(L"TcpTest\n");

  UINTN NoHandles;
  EFI_HANDLE *Handles;
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiTcp4ServiceBindingProtocolGuid,
    NULL,
    &NoHandles,
    &Handles);
  if (EFI_ERROR(Status)) {
    Print(L"LocateHandleBuffer Tcp4Sb %r\n", Status);
  }
  Print(L"No Handles (Tcp4Sb): %lu\n", NoHandles);

  EFI_SERVICE_BINDING_PROTOCOL *Tcp4Sb;
  Status = gBS->HandleProtocol(
    Handles[0],
    &gEfiTcp4ServiceBindingProtocolGuid,
    (VOID **)&Tcp4Sb);
  if (EFI_ERROR(Status)) {
    Print(L"HandleProtocol Tcp4Sb %r\n", Status);
  }

  EFI_HANDLE TcpHandle = NULL;
  Status = Tcp4Sb->CreateChild(Tcp4Sb, &TcpHandle);
  if (EFI_ERROR(Status)) {
    Print(L"Tcp4Sb CreateChild %r\n", Status);
  }

  EFI_TCP4_PROTOCOL *Tcp4;
  Status = gBS->HandleProtocol(
    TcpHandle,
    &gEfiTcp4ProtocolGuid,
    (VOID **)&Tcp4);
  if (EFI_ERROR(Status)) {
    Print(L"HandleProtocol Tcp4 %r\n", Status);
  }

  EFI_PXE_BASE_CODE_PROTOCOL *PxeBc;
  Status = gBS->LocateProtocol(
    &gEfiPxeBaseCodeProtocolGuid,
    NULL,
    (VOID **)&PxeBc);
  if (EFI_ERROR(Status)) {
    Print(L"LocateProtocol PxeBc %r\n", Status);
  }

  if (!PxeBc->Mode->Started) {
    Print(L"PxeBc Not Started\n");
  }
  if (!PxeBc->Mode->DhcpAckReceived) {
    Print(L"PxeBc Not DhcpAckReceived\n", Status);
  }

  EFI_TCP4_CONFIG_DATA Config;
  Config.TypeOfService = 0;
  Config.TimeToLive = 100;
  Config.AccessPoint.UseDefaultAddress = FALSE;
  gBS->CopyMem(Config.AccessPoint.StationAddress.Addr, PxeBc->Mode->StationIp.v4.Addr, 4);
  gBS->CopyMem(Config.AccessPoint.SubnetMask.Addr, PxeBc->Mode->SubnetMask.v4.Addr, 4);
  if (!AsmRdRand16(&Config.AccessPoint.StationPort)) {
    Print(L"AsmRdRand16 Failed\n");
  }
  Config.AccessPoint.StationPort |= 0x4000;
  Config.AccessPoint.StationPort &= 0x7fff;
  gBS->CopyMem(Config.AccessPoint.RemoteAddress.Addr, PxeBc->Mode->DhcpAck.Dhcpv4.BootpSiAddr, 4);
  Config.AccessPoint.RemotePort = 80;
  Config.AccessPoint.ActiveFlag = TRUE;
  Config.ControlOption = NULL;
  Status = Tcp4->Configure(Tcp4, &Config);
  if (EFI_ERROR(Status)) {
    Print(L"Tcp4 Configure %r\n", Status);
  }

  EFI_TCP4_CONNECTION_TOKEN Token;
  Status = gBS->CreateEvent(
    EVT_NOTIFY_SIGNAL,
    TPL_CALLBACK,
    ConnectionCallBack,
    NULL,
    &Token.CompletionToken.Event);
  if (EFI_ERROR(Status)) {
    Print(L"CreateEvent Connection %r\n", Status);
  }
  Token.CompletionToken.Status = EFI_SUCCESS;
  Status = Tcp4->Connect(Tcp4, &Token);
  if (EFI_ERROR(Status)) {
    Print(L"Tcp4 Connect %r\n", Status);
  }

  while (!HasConnected) {
    Status = Tcp4->Poll(Tcp4);
    // if (Status != EFI_NOT_READY) {
    //   Print(L"Connection Poll %r\n", Status);
    //   break;
    // }
  }

  CHAR8 HtmlMessage[255];
  AsciiSPrint(HtmlMessage, 255,
              "GET /index.html HTTP/1.1\r\n"
              "Host: %u.%u.%u.%u\r\n"
              "\r\n",
              PxeBc->Mode->DhcpAck.Dhcpv4.BootpSiAddr[0],
              PxeBc->Mode->DhcpAck.Dhcpv4.BootpSiAddr[1],
              PxeBc->Mode->DhcpAck.Dhcpv4.BootpSiAddr[2],
              PxeBc->Mode->DhcpAck.Dhcpv4.BootpSiAddr[3]);
  UINT32 HtmlMessageLength = AsciiStrLen(HtmlMessage);

  EFI_TCP4_TRANSMIT_DATA TxData;
  TxData.Push = TRUE;
  TxData.Urgent = FALSE;
  TxData.DataLength = HtmlMessageLength;
  TxData.FragmentCount = 1;
  TxData.FragmentTable[0].FragmentLength = HtmlMessageLength;
  TxData.FragmentTable[0].FragmentBuffer = HtmlMessage;

  EFI_TCP4_IO_TOKEN TransmitToken;
  TransmitToken.CompletionToken.Status = EFI_SUCCESS;
  Status = gBS->CreateEvent(
    EVT_NOTIFY_SIGNAL,
    TPL_CALLBACK,
    TransmitCallBack,
    NULL,
    &TransmitToken.CompletionToken.Event);
  if (EFI_ERROR(Status)) {
    Print(L"CreateEvent Transmit %r\n", Status);
  }
  TransmitToken.Packet.TxData = &TxData;

  Status = Tcp4->Transmit(Tcp4, &TransmitToken);
  if (EFI_ERROR(Status)) {
    Print(L"Transmit %r\n", Status);
  }

  while (!HasTransmitted) {
    Tcp4->Poll(Tcp4);
  }

  CHAR8 Buffer[2000];
  EFI_TCP4_RECEIVE_DATA RxData;
  RxData.DataLength = 2000;
  RxData.FragmentCount = 1;
  RxData.FragmentTable[0].FragmentLength = 2000;
  RxData.FragmentTable[0].FragmentBuffer = Buffer;

  EFI_TCP4_IO_TOKEN ReceiveToken;
  ReceiveToken.CompletionToken.Status = EFI_SUCCESS;
  ReceiveToken.Packet.RxData = &RxData;

  BOOLEAN IsHeader = TRUE;
  for (INT32 i = 0; i < 8; ++i) {
    Status = gBS->CreateEvent(
      EVT_NOTIFY_SIGNAL,
      TPL_CALLBACK,
      ReceiveCallBack,
      &ReceiveToken,
      &ReceiveToken.CompletionToken.Event);
    if (EFI_ERROR(Status)) {
      Print(L"CreateEvent Receive %r\n", Status);
    }
    RxData.DataLength = 2000;
    RxData.FragmentTable[0].FragmentLength = 2000;

    Print(L"\n");
    HasReceived = FALSE;

    Status = Tcp4->Receive(Tcp4, &ReceiveToken);
    if (EFI_ERROR(Status)) {
      Print(L"Receive %r\n", Status);
    }

    while (!HasReceived) {
      Tcp4->Poll(Tcp4);
      //Status = Tcp4->Poll(Tcp4);
      // if (Status != EFI_NOT_READY) {
      //  Print(L"Receive Poll %r: ", Status);
      //  break;
      //}
    }

    Print(L"DataLength: %u\n", ReceiveToken.Packet.RxData->DataLength);
    // Print(L"FragmentCount: %u\n", ReceiveToken.Packet.RxData->FragmentCount);
    // Print(L"Fragment[0] Size: %u\n", ReceiveToken.Packet.RxData->FragmentTable[0].FragmentLength);

    IsHeader = OutputHttp(Buffer, RxData.FragmentTable[0].FragmentLength, IsHeader);

  }


  Status = Tcp4->Poll(Tcp4);
  Print(L"\n%r\n", Status);

  Print(L"Press Any Key to Reset");
  WaitForKey();
  gST->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);

  return EFI_SUCCESS;
}
