### KicadHelper

This tool 'helps' me manager/fix Kicad Projects.

First Tab does find/replace in selected file.
Second Tab will find missing footprints.
Third Tab will add Digikey, LCSC, MPN Partnumbers based on the Value.(Ignores Footprint ATM)
Forth Tab with Copy/Rename Kicad Files for generating new Kicad Project based on an existing design.


### Commandline Interface

Example:
Opens Project
Checks Footprints
Saves Footprints Results
Exits

KicadHelper.exe -p "D:\PB_16\PB_16v1\PB_16.kicad_pro" -c -o "D:\report.txt" -x