# Live-game smoke for "the windows behind the game freeze".
#  1 Chrome, Edge, Discord and other Chromium/Electron apps pause video and
#    stop painting when a visible window that they judge fully opaque covers
#    them. They only read Win32 styles, so the overlay must be a layered window
#    with whole-window alpha below 255 (or a colour key); DWM per-pixel alpha
#    alone reads as opaque.
#  2 Live wallpapers (Wallpaper Engine) pause while the foreground window's
#    rect covers the whole monitor, so the overlay stops one row short of the
#    bottom edge; that row must still belong to the game (the click guard), or
#    a click there would land on the desktop.
#  3 If Wallpaper Engine is running and busy, its GPU use must not collapse
#    while the game plays (skipped when it is idle or absent).
#
# Runs the game full-screen into combat and probes all of this while it plays.
# Needs build\windows\x64\Release\WiNILL.exe. Uses a throwaway save folder.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = Join-Path $root 'build\windows\x64\Release'
$exe = Join-Path $exeDir 'WiNILL.exe'
if (-not (Test-Path $exe)) { throw "build first: $exe" }

if (-not ('SeeThroughProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
public static class SeeThroughProbe {
    delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr p);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] static extern int GetWindowLongW(IntPtr h, int i);
    [DllImport("user32.dll")] static extern bool GetLayeredWindowAttributes(IntPtr h, out uint key, out byte alpha, out uint flags);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] static extern IntPtr MonitorFromWindow(IntPtr h, uint f);
    [DllImport("user32.dll")] static extern bool GetMonitorInfoW(IntPtr m, ref MONITORINFO mi);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct MONITORINFO { public int cbSize; public RECT rcMonitor, rcWork; public uint dwFlags; }

    // The game rect must not cover its monitor (live wallpapers would pause),
    // and the monitor's bottom row must still be one of the game's windows.
    public static string Edge(IntPtr h, uint pid, out bool ok) {
        RECT w; GetWindowRect(h, out w);
        var mi = new MONITORINFO(); mi.cbSize = Marshal.SizeOf(mi);
        GetMonitorInfoW(MonitorFromWindow(h, 1), ref mi);
        RECT m = mi.rcMonitor;
        bool covers = w.L <= m.L && w.T <= m.T && w.R >= m.R && w.B >= m.B;
        var pt = new POINT { X = (m.L + m.R) / 2, Y = m.B - 1 };
        IntPtr hit = WindowFromPoint(pt);
        uint owner; GetWindowThreadProcessId(hit, out owner);
        bool guarded = owner == pid;
        // Another window brought over the game hides both rows: nothing to judge.
        var mid = new POINT { X = pt.X, Y = (m.T + m.B) / 2 };
        uint midOwner; GetWindowThreadProcessId(WindowFromPoint(mid), out midOwner);
        if (midOwner != pid) { ok = !covers; return "game not in front, bottom row not judged"; }
        ok = !covers && guarded;
        var cls = new System.Text.StringBuilder(64); GetClassNameW(hit, cls, 64);
        string who = guarded ? "" : " (" + cls + ", pid " + owner + ")";
        return (covers ? "covers monitor" : "short of monitor") + ", bottom row " + (guarded ? "guarded" : "OPEN" + who);
    }
    const int GWL_EXSTYLE = -20;
    const int WS_EX_LAYERED = 0x80000;
    const uint LWA_COLORKEY = 1, LWA_ALPHA = 2;

    // Largest GLFW window of the process (the overlay; boss panes are small).
    public static IntPtr FindOverlay(uint pid) {
        IntPtr best = IntPtr.Zero; long bestArea = 0;
        EnumWindows((h, p) => {
            uint owner; GetWindowThreadProcessId(h, out owner);
            if (owner != pid) return true;
            var cls = new System.Text.StringBuilder(64);
            GetClassNameW(h, cls, 64);
            if (!cls.ToString().StartsWith("GLFW")) return true;
            RECT r; GetWindowRect(h, out r);
            long area = (long)(r.R - r.L) * (r.B - r.T);
            if (area > bestArea) { bestArea = area; best = h; }
            return true;
        }, IntPtr.Zero);
        return best;
    }

    // Chromium's layered-window filter: a window is see-through to occlusion
    // checks only if it is layered with alpha < 255 or a colour key.
    public static string Describe(IntPtr h, out bool seeThrough) {
        int ex = GetWindowLongW(h, GWL_EXSTYLE);
        RECT r; GetWindowRect(h, out r);
        string size = (r.R - r.L) + "x" + (r.B - r.T);
        seeThrough = false;
        if ((ex & WS_EX_LAYERED) == 0)
            return "ex=0x" + ex.ToString("X8") + " size=" + size + " not layered (reads opaque)";
        uint key, flags; byte alpha;
        if (!GetLayeredWindowAttributes(h, out key, out alpha, out flags))
            return "ex=0x" + ex.ToString("X8") + " size=" + size + " layered, no attributes (reads opaque)";
        seeThrough = ((flags & LWA_ALPHA) != 0 && alpha < 255) || (flags & LWA_COLORKEY) != 0;
        return "ex=0x" + ex.ToString("X8") + " size=" + size + " layered alpha=" + alpha + " flags=" + flags;
    }
}
'@
}

# Wallpaper Engine GPU use (3D + video decode %), or -1 when it is not running.
function WallpaperGpu {
    $we = Get-Process wallpaper64, wallpaper32 -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $we) { return -1 }
    $c = (Get-Counter -ListSet 'GPU Engine').PathsWithInstances |
        Where-Object { $_ -match "pid_$($we.Id)_" -and $_ -match 'Utilization' -and $_ -match 'engtype_(3d|video)' }
    if (-not $c) { return -1 }
    $s = (Get-Counter -Counter $c -SampleInterval 1 -MaxSamples 2 -ErrorAction SilentlyContinue).CounterSamples
    [math]::Round((($s | Measure-Object CookedValue -Sum).Sum) / 2, 1)
}
$wallIdle = WallpaperGpu

$saveDir = Join-Path $env:TEMP "asteria-see-through-smoke-$PID"
New-Item -ItemType Directory -Force $saveDir | Out-Null
# Into combat, then keep the player alive for ~8 s while the probe looks.
$steps = @('RUN~0.3', 'LEVEL=80~0.05')
foreach ($i in 1..16) { $steps += 'HP=1~0.02'; $steps += 'MOBHP=1000000~0.48' }
$env:ASTERIA_SMOKE = $steps -join ';'
$env:ASTERIA_SMOKE_TAG = 'seethrough'
$env:ASTERIA_SAVE_DIR = $saveDir
$results = @()
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory $exeDir -PassThru
    $deadline = (Get-Date).AddSeconds(120)
    $hwnd = [IntPtr]::Zero
    while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
        if ($hwnd -eq [IntPtr]::Zero) { $hwnd = [SeeThroughProbe]::FindOverlay([uint32]$p.Id) }
        if ($hwnd -ne [IntPtr]::Zero) {
            $ok = $false; $edgeOk = $false
            $line = [SeeThroughProbe]::Describe($hwnd, [ref]$ok)
            $edge = [SeeThroughProbe]::Edge($hwnd, [uint32]$p.Id, [ref]$edgeOk)
            $results += [pscustomobject]@{ T = [math]::Round(((Get-Date) - $p.StartTime).TotalSeconds, 1); SeeThrough = $ok; EdgeOk = $edgeOk; State = $line; Edge = $edge }
        }
        Start-Sleep -Milliseconds 700
        if ($results.Count -eq 6 -and $wallIdle -gt 5) { $wallGame = WallpaperGpu }
    }
    if (-not $p.HasExited) { $p.Kill(); throw 'see-through smoke timed out' }
    if ($p.ExitCode -ne 0) { throw "see-through smoke exited with $($p.ExitCode)" }
} finally {
    Remove-Item Env:ASTERIA_SMOKE, Env:ASTERIA_SMOKE_TAG, Env:ASTERIA_SAVE_DIR -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $saveDir -ErrorAction SilentlyContinue
}

$results | Format-Table -AutoSize | Out-String -Width 250 | Write-Host
if ($results.Count -lt 3) { throw "probe saw the game window only $($results.Count) times" }
# A minimised window (160x28) says nothing about the overlay; judge the rest.
$live = @($results | Where-Object { $_.State -notmatch 'size=160x28' })
$bad = @($live | Where-Object { -not $_.SeeThrough })
if ($bad.Count -gt 0) {
    throw "FAIL: the game window read as an opaque cover $($bad.Count)/$($live.Count) times; apps behind it would stop painting"
}
$badEdge = @($live | Where-Object { -not $_.EdgeOk })
if ($badEdge.Count -gt 0) {
    throw "FAIL: $($badEdge[0].Edge) ($($badEdge.Count)/$($live.Count) probes); live wallpapers pause or clicks reach the desktop"
}
$guarded = @($live | Where-Object { $_.Edge -match 'guarded' }).Count
if ($guarded -lt 3) { throw "the game was in front for only $guarded probes; keep other windows away and run again" }
if ($wallIdle -lt 0) {
    Write-Host 'Wallpaper Engine: not running, live check skipped'
} elseif ($wallIdle -le 5) {
    Write-Host "Wallpaper Engine: idle before the run ($wallIdle% GPU), live check skipped"
} else {
    Write-Host "Wallpaper Engine GPU: $wallIdle% before, $wallGame% during the run"
    if ($null -eq $wallGame -or $wallGame -lt $wallIdle * 0.25) {
        throw "FAIL: Wallpaper Engine paused while the game ran ($wallIdle% -> $wallGame%)"
    }
}
Write-Host "PASS: the game window stayed see-through and short of the monitor edge ($($live.Count) probes)"
