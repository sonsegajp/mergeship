# MergeShip2: run a harness test while Windows keeps the display on (a sleeping display throttles the game's
# presentation to about a frame a second, which stalls tests run overnight).
# Usage: powershell -ExecutionPolicy Bypass -File tools/run_awake.ps1 <testdir> [secs] [exe]
param([string]$Test, [int]$Secs = 240, [string]$Exe = "soh_new.exe")
$sig = '[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint esFlags);'
$k = Add-Type -MemberDefinition $sig -Name "ExecAwake" -Namespace "MergeShip" -PassThru
$null = $k::SetThreadExecutionState([uint32]2147483651) # ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
$env:MS_EXE_DIR = "/c/Users/hyper/Mergeship2/Shipwright/x64/Release"
$env:MS_EXE = $Exe
& "C:\Program Files\Git\bin\bash.exe" -c "cd /c/Users/hyper/Mergeship2 && bash build/tests/run_test.sh build/tests/$Test $Secs"
$null = $k::SetThreadExecutionState([uint32]2147483648) # ES_CONTINUOUS: back to normal
