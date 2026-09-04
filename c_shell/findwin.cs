using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
class P {
    delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    static void Main() {
        int mcPid = 0;
        foreach (var p2 in Process.GetProcessesByName("mc_console")) mcPid = p2.Id;
        Console.WriteLine("mc_pid=" + mcPid);
        EnumWindows((h, l) => {
            var sb = new StringBuilder(256);
            GetClassName(h, sb, 256);
            string c = sb.ToString();
            uint pid; GetWindowThreadProcessId(h, out pid);
            if (c.IndexOf("MC", StringComparison.OrdinalIgnoreCase) >= 0 || c.IndexOf("Chrome", StringComparison.OrdinalIgnoreCase) >= 0 || (int)pid == mcPid)
                Console.WriteLine(string.Format("hwnd={0} pid={1} class={2}", h, pid, c));
            return true;
        }, IntPtr.Zero);
    }
}
