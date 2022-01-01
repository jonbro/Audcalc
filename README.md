# TDM

a pocket sized groovebox.

![prototype v6](Images/twrtdm_card.jpg)
*current prototype*

- RP2040 (overclocked to 300mhz)
- 1/8in audio i/o
- onboard mic
- 104mm x 68mm x 14mm

The bom is contained within the Hardware kicad project, ready for assembly via jlcpcb.
There are several additional items that must be sourced and handsoldered:

| part                         | count | mfg part number   | link                                                                                                   | notes                                                                                 |
|------------------------------|-------|-------------------|--------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------|
| battery connector            | 1     | jst s2b-ph-sm4-tb | https://octopart.com/s2b-ph-sm4-tb%28lf%29%28sn%29-jst-248913                                          | these are redibly available from aliexpress as well                                   |
| midi / audio jacks           | 4     | PJ-327C-4A        | https://lcsc.com/product-detail/Audio-Connectors_Korean-Hroparts-Elec-PJ-327C-4A_C145813.html          | I believe there is an equivalent CUI headphone jack. I'm using this cheaper version   |
| pots                         | 2     | alps RK09K1130A5R | https://lcsc.com/product-detail/Variable-Resistors-Potentiometers_ALPSALPINE-RK09K1130A5R_C209779.html |                                                                                       |
| oled screen (128x32)         | 1     | ssd1306           | https://www.aliexpress.com/item/4000842671330.html                                                     | I'm using the white / solderable version.                                             |
| battery                      | 1     | lp-503562         | https://www.adafruit.com/product/258                                                                   | There are other cheaper sources for these. Confirm the correct polarity before using. |
| m2 4mm f-f standoff          | 4     |                   | https://www.aliexpress.com/item/1005002145042844.html                                                  |                                                                                       |
| m2 6mm m-f standoff          | 4     |                   | https://www.aliexpress.com/item/1005002145042844.html                                                  |                                                                                       |
| m2 3mm hex head bolt (black) | 8     |                   | https://www.aliexpress.com/item/10000148429238.html                                                    |                                                                                       |
