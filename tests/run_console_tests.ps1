# Valida os dois modos com stdin redirecionado, incluindo pipe aberto sem dados.
param(
    [string]$Configuration = "Debug",
    [string]$CompilerPath = "",
    [string]$VmPath = ""
)
$ErrorActionPreference = "Stop"
$ProjectRoot = (Get-Item $PSScriptRoot).Parent.FullName
if (-not $CompilerPath) { $CompilerPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl.exe" }
if (-not $VmPath) { $VmPath = Join-Path $ProjectRoot "bin\$Configuration\ijkl-m.exe" }
$TemporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $TemporaryDirectory | Out-Null
$Passed = 0
$Outputs = @{}
if (-not ([System.Management.Automation.PSTypeName]"Win32ProcessRunner").Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public class Win32ProcessRunner {
    [StructLayout(LayoutKind.Sequential)]
    public struct SECURITY_ATTRIBUTES {
        public int nLength;
        public IntPtr lpSecurityDescriptor;
        public bool bInheritHandle;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct STARTUPINFO {
        public int cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute;
        public int dwFlags;
        public short wShowWindow, cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_INFORMATION {
        public IntPtr hProcess;
        public IntPtr hThread;
        public int dwProcessId;
        public int dwThreadId;
    }

    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool CreatePipe(out IntPtr hReadPipe, out IntPtr hWritePipe, ref SECURITY_ATTRIBUTES lpPipeAttributes, int nSize);

    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool SetHandleInformation(IntPtr hObject, int dwMask, int dwFlags);

    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern bool CreateProcess(string lpApplicationName, string lpCommandLine, IntPtr lpProcessAttributes, IntPtr lpThreadAttributes, bool bInheritHandles, int dwCreationFlags, IntPtr lpEnvironment, string lpCurrentDirectory, ref STARTUPINFO lpStartupInfo, out PROCESS_INFORMATION lpProcessInformation);

    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool WriteFile(IntPtr hFile, byte[] lpBuffer, int nNumberOfBytesToWrite, out int lpNumberOfBytesWritten, IntPtr lpOverlapped);

    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool ReadFile(IntPtr hFile, byte[] lpBuffer, int nNumberOfBytesToRead, out int lpNumberOfBytesRead, IntPtr lpOverlapped);

    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern int WaitForSingleObject(IntPtr hHandle, int dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool GetExitCodeProcess(IntPtr hProcess, out int lpExitCode);

    public static string RunWithPipe(string exePath, string args, byte[] inputBytes, bool closeInput, int timeoutMs) {
        SECURITY_ATTRIBUTES sa = new SECURITY_ATTRIBUTES();
        sa.nLength = Marshal.SizeOf(sa);
        sa.bInheritHandle = true;
        sa.lpSecurityDescriptor = IntPtr.Zero;

        IntPtr inRead, inWrite, outRead, outWrite;
        CreatePipe(out inRead, out inWrite, ref sa, 0);
        CreatePipe(out outRead, out outWrite, ref sa, 0);
        SetHandleInformation(inWrite, 1, 0);
        SetHandleInformation(outRead, 1, 0);

        STARTUPINFO si = new STARTUPINFO();
        si.cb = Marshal.SizeOf(si);
        si.dwFlags = 0x00000100;
        si.hStdInput = inRead;
        si.hStdOutput = outWrite;
        si.hStdError = outWrite;

        PROCESS_INFORMATION pi;
        string cmdLine = "\"" + exePath + "\" " + args;
        bool ok = CreateProcess(null, cmdLine, IntPtr.Zero, IntPtr.Zero, true, 0, IntPtr.Zero, null, ref si, out pi);
        CloseHandle(inRead);
        CloseHandle(outWrite);

        if (!ok) {
            CloseHandle(inWrite);
            CloseHandle(outRead);
            throw new Exception("CreateProcess failed: " + Marshal.GetLastWin32Error());
        }

        if (inputBytes != null && inputBytes.Length > 0) {
            int written;
            WriteFile(inWrite, inputBytes, inputBytes.Length, out written, IntPtr.Zero);
        }
        if (closeInput) {
            CloseHandle(inWrite);
        }

        int waitRes = WaitForSingleObject(pi.hProcess, timeoutMs);
        if (!closeInput) {
            CloseHandle(inWrite);
        }

        StringBuilder sb = new StringBuilder();
        byte[] buffer = new byte[4096];
        int read;
        while (ReadFile(outRead, buffer, buffer.Length, out read, IntPtr.Zero) && read > 0) {
            sb.Append(Encoding.Default.GetString(buffer, 0, read));
        }
        CloseHandle(outRead);

        int exitCode;
        GetExitCodeProcess(pi.hProcess, out exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (waitRes != 0) {
            throw new Exception("Process timed out after " + timeoutMs + " ms");
        }
        if (exitCode != 0) {
            throw new Exception("Process exited with code " + exitCode + ": " + sb.ToString());
        }
        return sb.ToString().Replace("\r\n", "\n");
    }
}
"@
}

function Invoke-ConsoleProbe {
    param([string]$ProgramPath, [string]$InputText, [bool]$CloseInput = $true)
    $Bytes = [System.Text.Encoding]::GetEncoding("iso-8859-1").GetBytes($InputText)
    return [Win32ProcessRunner]::RunWithPipe($VmPath, "`"$ProgramPath`"", $Bytes, $CloseInput, 5000)
}
try {
    foreach ($Mode in @("ci", "native")) {
        $Programs = @{}
        foreach ($Fixture in @("probe", "mixed", "aliases", "poll")) {
            $Target = Join-Path $TemporaryDirectory "$Fixture-$Mode.ci"
            $Language = if ($Fixture -eq "aliases") { "pt" } else { "en" }
            $CompilerArguments = @("-ini", (Join-Path $PSScriptRoot "ijkl_$Language.ini"))
            if ($Mode -eq "native") { $CompilerArguments += "--native-builtins" }
            $CompilerArguments += @((Join-Path $PSScriptRoot "console\$Fixture.ijk"), "-o", $Target)
            Push-Location $ProjectRoot
            try {
                & $CompilerPath @CompilerArguments
                if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar $Fixture ($Mode)" }
            } finally { Pop-Location }
            $Emitted = [IO.File]::ReadAllText($Target)
            foreach ($Builtin in @("has_key", "read_key_async")) {
                if ($Emitted.Contains("call #__builtin_console_$Builtin;") -ne ($Mode -eq "native")) {
                    throw "Despacho incorreto: $Fixture, $Mode, $Builtin"
                }
            }
            if ($Mode -eq "ci" -and (-not $Emitted.Contains("in 0x0005, r0;") -or -not $Emitted.Contains("in 0x0006, r0;"))) {
                throw "Implementacao CI ausente"
            }
            $Programs[$Fixture] = $Target
        }
        $Cases = @(
            @{ Name = "empty"; Fixture = "probe"; InputText = ""; Expected = "0`n0`n0`n0`n0`n0`n0`n" },
            @{ Name = "buffered"; Fixture = "probe"; InputText = "AB"; Expected = "1`n1`n65`n1`n66`n0`n0`n" },
            @{ Name = "escape"; Fixture = "probe"; InputText = [string][char]27; Expected = "1`n1`n27`n0`n0`n0`n0`n" },
            @{ Name = "nul"; Fixture = "probe"; InputText = [string][char]0; Expected = "1`n1`n0`n0`n0`n0`n0`n" },
            @{ Name = "mixed"; Fixture = "mixed"; InputText = "hello`nZ"; Expected = "1`nhello`n1`n90`n" },
            @{ Name = "aliases"; Fixture = "aliases"; InputText = "Z"; Expected = "1`n90`n" }
        )
        foreach ($Case in $Cases) {
            $Actual = Invoke-ConsoleProbe -ProgramPath $Programs[$Case.Fixture] -InputText $Case.InputText
            if ($Actual -cne $Case.Expected) { throw "Saida incorreta: $Mode / $($Case.Name): $Actual" }
            if ($Mode -eq "native" -and $Actual -cne $Outputs[$Case.Name]) { throw "Paridade divergente" }
            $Outputs[$Case.Name] = $Actual
            $Passed++
            Write-Host "PASS ${Mode}: $($Case.Name)"
        }
        $Actual = Invoke-ConsoleProbe -ProgramPath $Programs["probe"] -InputText "" -CloseInput $false
        if ($Actual -cne $Cases[0].Expected) { throw "Pipe aberto sem dados retornou resultado incorreto" }
        $Passed++
        Write-Host "PASS ${Mode}: open-empty-pipe"
    }
    Write-Host "Console: $Passed passed; CI/native parity verified"
} finally {
    Remove-Item -LiteralPath $TemporaryDirectory -Recurse -Force
}
