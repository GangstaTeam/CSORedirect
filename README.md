# Script Redirect
Small plugin that redirects files based on lists of wildcard/files from CSO/DSO to drive folder.

## Usage
- You will need create file under plugins folder with name `ScriptRedirect.list`.
  - This file will contain file list with full path that you will want to redirect from CSO/DSO.
  - You can use wildcard to redirect all files inside folder as `scriptc\*` or `scriptc\*.cso/dso`.
 
## Configuration
- ScriptRedirect.ini
```
# ScriptRedirect.ini
# Toggle debug console for script output
DebugConsole=1
```
Native `echo("[DEBUG]", "Example debug message");` can be used for debug messages in the console.
