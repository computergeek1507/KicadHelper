### KicadHelper

This tool 'helps' me manager/fix Kicad Projects.

First Tab does find/replace in selected file.

![Find Replace Tab](/res/images/2022-08-27_10h01_03.png)

Second Tab will find missing footprints.
Third Tab will find missing symbols.
Forth Tab will add Digikey, LCSC, MPN Partnumbers based on the Value and Footprint.
Fifth Tab with Copy/Rename Kicad Files for generating new Kicad Project based on an existing design.


### Commandline Interface

Example:
Opens Project
Checks Footprints/Symbols
Saves Results to file
Exits

KicadHelper.exe -p "D:\PB_16\PB_16v1\PB_16.kicad_pro" -c -o "D:\report.txt" -x