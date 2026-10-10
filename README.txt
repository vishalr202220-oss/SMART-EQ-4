SMART EQ PRODUCERTOY - READY VST3 BANANE KA ASAAN TARIKA (PC pe kuch install nahi karna)

1. github.com pe free account banao -> New repository (naam: SmartEQ).
2. Is zip ke andar ka sab kuch (CMakeLists.txt, Source folder, .github folder) repo me upload karo
   ("Add file" -> "Upload files"). .github folder bhi upload hona chahiye.
3. Repo me "Actions" tab kholo -> build chalega (10-15 min).
4. Build green hone par us run pe click karo -> neeche "Artifacts" me
   SMART-EQ-PRODUCERTOY-windows-latest download karo (Mac ke liye macos-latest).
5. Zip kholo, "SMART EQ PRODUCERTOY.vst3" ko yahan copy karo:
   Windows: C:\Program Files\Common Files\VST3
   Mac:     /Library/Audio/Plug-Ins/VST3
6. DAW me plugin rescan karo.

Local build (optional): cmake -B build && cmake --build build --config Release
