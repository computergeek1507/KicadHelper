### KicadHelper

This tool 'helps' me manager/fix Kicad Projects.

![Gif]("/res/images/find fp.gif")

First Tab Tab will add Digikey, LCSC, MPN Partnumbers based on the Value and Footprint.

![Update Partnumbers Tab](/res/images/2022-08-27_10h01_27.png)

Second Tab will find missing footprints.

![Find Footprints Tab](/res/images/2022-08-27_10h01_54.png)

Third Tab will find missing symbols.

![Find Symbols Tab](/res/images/2022-08-27_10h39_02.png)

Forth Tab will does find/replace in selected file.

![Find Replace Tab](/res/images/2022-08-27_10h02_17.png)

Fifth Tab with Copy/Rename Kicad Files for generating new Kicad Project based on an existing design.

![Rename Tab](/res/images/2022-08-27_10h02_25.png)

### Commandline Interface

Example:
Opens Project
Checks Footprints/Symbols
Saves Results to file
Exits

KicadHelper.exe -p "D:\PB_16\PB_16v1\PB_16.kicad_pro" -c -o "D:\report.txt" -x