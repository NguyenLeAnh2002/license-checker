License Checker Agent - Deployment to another machine (VMware VM)
===========================================================================

Files in this folder:
  LicenseCheckerAgent.exe       - the agent, already built (static runtime,
                                   no separate VC++ redistributable needed)
  agent_config.wifi.json        - Bridged mode, VM IP starts with 192.168.99.
  agent_config.ethernet.json    - Bridged mode, VM IP starts with 10.1.65.
  agent_config.nat.json         - NAT mode (see below) - RECOMMENDED, doesn't
                                   depend on Wi-Fi supporting bridging.

Which networking mode to use
-----------------------------
NAT is simpler and more reliable than Bridged (bridging often fails on
Wi-Fi adapters). If your VM's network adapter is set to NAT in VMware's VM
settings, use agent_config.nat.json - it's pre-set to 192.168.41.1, which
is VMware's fixed NAT gateway address for reaching this host from inside
the VM (already confirmed reachable: curl http://192.168.41.1:8000/api/health
returns 200 from this host).

IMPORTANT: use 192.168.41.1 (the host, via the NAT gateway), NOT the VM's
own IP (e.g. 192.168.41.129) - the VM's own address has nothing listening
on it and will always fail to connect.

Steps
-----

1. On the HOST (this machine), open the firewall for the server port.
   Run this in an ELEVATED PowerShell (this step must be done by you,
   not automated):

       New-NetFirewallRule -DisplayName "License Checker Server" -Direction Inbound -Protocol TCP -LocalPort 8000 -Action Allow

   Make sure the mock server is running on this host:
       cd E:\License-checker\server
       python -m app.main

2. Pick the config based on your VM's networking mode:
     - VM set to NAT in VMware settings -> use agent_config.nat.json
       (points to 192.168.41.1, the host's NAT gateway address).
     - VM set to Bridged, IP starts with 192.168.99.x -> agent_config.wifi.json
     - VM set to Bridged, IP starts with 10.1.65.x    -> agent_config.ethernet.json
     - None of these match -> tell Claude the VM's IP/subnet (run `ipconfig`
       inside the VM) so it can help pick the right host address.

3. On the VM, put both copied files in the same folder, then RENAME
   whichever config file you copied to exactly:
       agent_config.json

4. Test the connection from the VM first (before running the agent):
       curl http://<host-ip>:8000/api/health
   This must return a 200 / JSON health response. If it times out or is
   refused, re-check step 1 (firewall) and that the server is running.

5. Run the agent:
       LicenseCheckerAgent.exe
   It runs in console mode (no service install needed for a quick test),
   detects license status, and reports to the server every cycle (first
   one runs immediately). Check license-detection.log next to the exe for
   a line like:
       [INFO] ServerReporter: report sent successfully to http://<host-ip>:8000

6. On the host, open http://localhost:8000/dashboard -> Machines tab and
   confirm the VM shows up as a new machine, identified by its own
   hostname and machine GUID. Click it to see its Windows/Office license
   detail.

For a persistent (always-on) deployment instead of running it manually,
install it as a Windows service on the VM (run as Administrator):
       LicenseCheckerAgent.exe install "C:\path\to\LicenseCheckerAgent.exe"
       sc start LicenseCheckerAgent
