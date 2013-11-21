假设有两台机器：
Test Machine（用于测试的，driver安装在这台机器上）
Build Machine（用于编译driver的机器）

1）Enable the Test Mode of the Test Machine
   Launch a command prompt and issue the following command, then restart the machine:
   bcdedit /set testsigning on

2) Create a test certification on Build Machine
   Open a "x64 free build environment" command prompt from the WDK;
   Issue following command:
    MakeCert /r /pe /ss TestStore /n "CN=IBM SSD Cache Mgr(Test)" ibmssd_test.cer
   Then the ibmssd_test.cer will be generated.

3) Build the device driver
   
4) Sign the device driver
    SignTool sign /v /a /s TestStore /n "IBM SSD Cache Mgr(Test)" xxxx.sys
    Inf2Cat /driver:Driver_Path /os:Server2008R2_X64
    SignTool sign /v /a /s TestStore /n "IBM SSD Cache Mgr(Test)" xxxx.cat
   (Please replace the xxxx.sys, xxxx.cat, Driver_Path with the actual name)

Step 2) - 4) is done on the Build Machine. And just do Step 2) once.
You can write a script to do Step 3) and Step 4) for build.