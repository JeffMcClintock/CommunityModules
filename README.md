# modules
SynthEdit modules
Please create a directory for your own modules. Don't mess with other peoples modules.

To create a Visual Studio Project use CMake GUI. Set the source code location to the 'CommunityModules' folder. Set where to build the binaries as a folder outside the CommunityModules folder e.g. 'C:\SE\build_CM'. HIt 'Configure', then 'Generate', 'Open Project'

You can set the 'Post Build Event' as: copy "$(OutDir)$(TargetName)$(TargetExt)" "C:\Program Files\Common Files\SynthEdit\modules-staged\"
to automatically copy your module to the staging folder for hot-reload.