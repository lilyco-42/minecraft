Add-Type @'
using System;
using System.Runtime.InteropServices;
public class WT3 {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr p, IntPtr a, string cls, string title);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
}
'@
$h = [IntPtr]::Zero
$n = 0
while ($n -lt 20) {
  $h = [WT3]::FindWindowEx([IntPtr]::Zero, $h, "Chrome_WidgetWin_1", $null)
  if ($h -eq [IntPtr]::Zero) { break }
  $pid2 = [uint32]0
  [void][WT3]::GetWindowThreadProcessId($h, [ref]$pid2)
  $proc = Get-Process -Id $pid2 -ErrorAction SilentlyContinue
  $parent = [WT3]::GetParent($h)
  $pname = if ($proc) { $proc.ProcessName } else { "?" }
  Write-Output ("{0} hwnd={1} parent={2}" -f $pname, $h.ToString(), $parent.ToString())
  $n++
}
