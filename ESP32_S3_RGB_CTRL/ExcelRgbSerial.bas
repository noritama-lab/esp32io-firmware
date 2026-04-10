Option Explicit

Private Const COM_PORT As String = "COM4"

Private Declare PtrSafe Function CreateFile Lib "kernel32" Alias "CreateFileA" (ByVal lpFileName As String, ByVal dwDesiredAccess As Long, ByVal dwShareMode As Long, ByVal lpSecurityAttributes As LongPtr, ByVal dwCreationDisposition As Long, ByVal dwFlagsAndAttributes As Long, ByVal hTemplateFile As LongPtr) As LongPtr
Private Declare PtrSafe Function CloseHandle Lib "kernel32" (ByVal hObject As LongPtr) As Long
Private Declare PtrSafe Function GetCommState Lib "kernel32" (ByVal hFile As LongPtr, lpDCB As DCB) As Long
Private Declare PtrSafe Function SetCommState Lib "kernel32" (ByVal hFile As LongPtr, lpDCB As DCB) As Long
Private Declare PtrSafe Function SetCommTimeouts Lib "kernel32" (ByVal hFile As LongPtr, lpCommTimeouts As COMMTIMEOUTS) As Long
Private Declare PtrSafe Function WriteFile Lib "kernel32" (ByVal hFile As LongPtr, ByVal lpBuffer As String, ByVal nNumberOfBytesToWrite As Long, lpNumberOfBytesWritten As Long, ByVal lpOverlapped As LongPtr) As Long
Private Declare PtrSafe Function ReadFile Lib "kernel32" (ByVal hFile As LongPtr, lpBuffer As Any, ByVal nNumberOfBytesToRead As Long, lpNumberOfBytesRead As Long, ByVal lpOverlapped As LongPtr) As Long

Private Type DCB
    DCBlength As Long
    BaudRate As Long
    fBitFields As Long
    wReserved As Integer
    XonLim As Integer
    XoffLim As Integer
    ByteSize As Byte
    Parity As Byte
    StopBits As Byte
    XonChar As Byte
    XoffChar As Byte
    ErrorChar As Byte
    EofChar As Byte
    EvtChar As Byte
    wReserved1 As Integer
End Type

Private Type COMMTIMEOUTS
    ReadIntervalTimeout As Long
    ReadTotalTimeoutMultiplier As Long
    ReadTotalTimeoutConstant As Long
    WriteTotalTimeoutMultiplier As Long
    WriteTotalTimeoutConstant As Long
End Type

Private Const GENERIC_READ As Long = &H80000000
Private Const GENERIC_WRITE As Long = &H40000000
Private Const OPEN_EXISTING As Long = 3
Private Const DCB_BINARY As Long = &H1
Private Const DCB_DTR_CONTROL_ENABLE As Long = &H10
Private Const DCB_RTS_CONTROL_ENABLE As Long = &H1000
Private Const INVALID_HANDLE_VALUE As LongPtr = -1

Public Sub PingEsp32()
    SendJsonAndShowResponse "{""cmd"":""ping""}"
End Sub

Public Sub SendA1ColorToEsp32()
    Dim targetCell As Range
    Dim colorValue As Long
    Dim redValue As Long
    Dim greenValue As Long
    Dim blueValue As Long
    Dim payload As String

    Set targetCell = ActiveSheet.Range("A1")

    If targetCell.Interior.Pattern = xlPatternNone Then
        payload = "{""cmd"":""led_off""}"
    Else
        colorValue = targetCell.Interior.Color
        redValue = colorValue And &HFF&
        greenValue = (colorValue \ &H100&) And &HFF&
        blueValue = (colorValue \ &H10000) And &HFF&
        payload = "{""cmd"":""set_rgb"",""r"":" & redValue & ",""g"":" & greenValue & ",""b"":" & blueValue & "}"
    End If

    SendJsonAndShowResponse payload
End Sub

Private Sub SendJsonAndShowResponse(ByVal payload As String)
    Dim handle As LongPtr
    Dim lineToSend As String
    Dim bytesWritten As Long
    Dim response As String
    Dim sentAt As String

    On Error GoTo ErrHandler

    handle = OpenConfiguredPort(COM_PORT)

    lineToSend = payload & vbLf
    If WriteFile(handle, lineToSend, LenB(StrConv(lineToSend, vbFromUnicode)), bytesWritten, 0) = 0 Then
        Err.Raise vbObjectError + 4001, "SendJsonAndShowResponse", "WriteFile failed"
    End If

    response = ReadLineFromPort(handle, 1000)
    sentAt = Format$(Now, "yyyy-mm-dd hh:nn:ss")

    If Len(response) > 0 Then
        Application.StatusBar = "ESP32 response " & sentAt & " " & response
        Debug.Print sentAt & " RECV(" & COM_PORT & "): " & response
    Else
        Application.StatusBar = "ESP32 no response " & sentAt
        Debug.Print sentAt & " RECV(" & COM_PORT & "): <none>"
    End If

    CloseHandle handle
    Exit Sub

ErrHandler:
    On Error Resume Next
    If handle <> 0 And handle <> INVALID_HANDLE_VALUE Then CloseHandle handle
    Application.StatusBar = "ESP32 send/recv error: " & Err.Description
    Debug.Print "SERIAL ERROR(" & COM_PORT & "): " & Err.Description
End Sub

Private Function OpenConfiguredPort(ByVal portName As String) As LongPtr
    Dim dcbState As DCB
    Dim timeouts As COMMTIMEOUTS
    Dim handle As LongPtr
    Dim portPath As String

    portPath = "\\.\" & portName
    handle = CreateFile(portPath, GENERIC_READ Or GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0)
    If handle = INVALID_HANDLE_VALUE Then
        Err.Raise vbObjectError + 4010, "OpenConfiguredPort", "Could not open " & portName
    End If

    dcbState.DCBlength = LenB(dcbState)
    If GetCommState(handle, dcbState) = 0 Then
        Err.Raise vbObjectError + 4011, "OpenConfiguredPort", "GetCommState failed"
    End If

    dcbState.BaudRate = 115200
    dcbState.fBitFields = DCB_BINARY Or DCB_DTR_CONTROL_ENABLE Or DCB_RTS_CONTROL_ENABLE
    dcbState.ByteSize = 8
    dcbState.Parity = 0
    dcbState.StopBits = 0
    If SetCommState(handle, dcbState) = 0 Then
        Err.Raise vbObjectError + 4012, "OpenConfiguredPort", "SetCommState failed"
    End If

    timeouts.ReadIntervalTimeout = 20
    timeouts.ReadTotalTimeoutMultiplier = 0
    timeouts.ReadTotalTimeoutConstant = 20
    timeouts.WriteTotalTimeoutMultiplier = 0
    timeouts.WriteTotalTimeoutConstant = 100
    If SetCommTimeouts(handle, timeouts) = 0 Then
        Err.Raise vbObjectError + 4013, "OpenConfiguredPort", "SetCommTimeouts failed"
    End If

    OpenConfiguredPort = handle
End Function

Private Function ReadLineFromPort(ByVal handle As LongPtr, ByVal timeoutMs As Long) As String
    Dim buf(0 To 0) As Byte
    Dim bytesRead As Long
    Dim startedAt As Single
    Dim elapsedMs As Long
    Dim lineText As String
    Dim ch As String

    startedAt = Timer
    Do
        If ReadFile(handle, buf(0), 1, bytesRead, 0) = 0 Then Exit Do
        If bytesRead > 0 Then
            ch = Chr$(buf(0))
            If ch = vbLf Then Exit Do
            If ch <> vbCr Then lineText = lineText & ch
        End If

        elapsedMs = CLng((Timer - startedAt) * 1000)
        If elapsedMs >= timeoutMs Then Exit Do
        DoEvents
    Loop

    ReadLineFromPort = Trim$(lineText)
End Function
