# pihpsdr

**piHPSDR Manual: (Release 2.4 Version) ** https://github.com/dl1ycf/pihpsdr/releases/download/v2.4/piHPSDR-Manual.pdf

**piHPSDR Manual: (current Version) ** https://github.com/dl1ycf/pihpsdr/releases/download/v2.4/piHPSDR-Manual-current.pdf

Standalone code for HPSDR,
supporting both the old (P1) and new (P2) HPSDR protocols, as well as the SoapySDR framework.

It runs on Linux (including RaspPi 3/4/5) and MacOS (using the "Homebrew" working environment).

***
Consult the Manual (**link given above**) on how to compile/install piHPSDR on your machine

-Appendix J: LINUX compile from sources (including RaspPi)

-Appendix K: MacOS compile from sources
***

Latest features:

- **client/server model for remote operation** (including transmitting in phone and CW)
- full support for Anan G2-Ultra radios, including customizable panel button/encoder functions
- added continuous frequency compressor (**CFC**) and downward expander (**DEXP**) to the TX chain
- in-depth manual (**link given above**)
- HermesLite-II I/O-board support
- audio recording (RX capture) and playback (TX)
- repository shrinked quite a bit by removing some binary files (much better now if you use WLAN)
- automatic installation procedures for compilation from the sources, for Linux (including RaspPi) and MacOS
  (Appendix J, K).
- dynamic screen resizing in the "Screen" menu, including transitions
  between full-screen and window mode
- CW audio peak filter (in the Filter menu)


