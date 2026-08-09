FOUR COLOR — macOS install
Multiband Colour & Saturation · by Gussa Naaman

------------------------------------------------------------------------------
INSTALL
------------------------------------------------------------------------------

  Double-click  FourColor-1.0.0-rc.1-macOS.pkg  and follow it.

  It installs, and you can untick any of the three:

    VST3        -> /Library/Audio/Plug-Ins/VST3
    Audio Unit  -> /Library/Audio/Plug-Ins/Components
    Standalone  -> /Applications

  Universal binary: Apple Silicon and Intel.

  Then start your DAW and rescan plugins. FOUR COLOR appears under
  Naaman, category Distortion.

  Cubase:  Studio > VST Plug-in Manager > Update
  Logic:   it rescans Audio Units on launch

------------------------------------------------------------------------------
IF macOS REFUSES TO OPEN IT
------------------------------------------------------------------------------

This build is signed ad-hoc but NOT notarised, so Gatekeeper may complain the
first time. That is expected for a development build and does not mean
anything is wrong with it.

  "FourColor-1.0.0-rc.1-macOS.pkg cannot be opened because it is from an
   unidentified developer"

    Right-click the .pkg -> Open -> Open. You only do this once.

  Or, if System Settings offers it:
    System Settings > Privacy & Security > scroll down > "Open Anyway"

  If the plug-in installs but the DAW will not load it, clear the quarantine
  flag in Terminal:

    sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/FourColor.vst3
    sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/FourColor.component

------------------------------------------------------------------------------
UNINSTALL
------------------------------------------------------------------------------

  Delete these, then rescan:

    /Library/Audio/Plug-Ins/VST3/FourColor.vst3
    /Library/Audio/Plug-Ins/Components/FourColor.component
    /Applications/FourColor.app

------------------------------------------------------------------------------
WHAT IT IS
------------------------------------------------------------------------------

Four bands, four colour engines, and one axis that decides whether the
saturation lands on the hit or on the body.

  WARM   round, fat, gently compressing
  IRON   dense and weighted - a saturator inside a feedback loop
  BITE   forward, present, upper-mid grit
  FUZZ   the broken one, and still usable at low amounts

  DRIVE            how hard the band is pushed
  BEHAVIOR         BODY <-> ATTACK
  TONE             dark <-> bright, around the band's own centre
  SPACE / SPREAD   diffuses only what the saturation created, never the source

Latency is 65 samples in every Quality setting, so changing Quality mid-song
never shifts the timing.

------------------------------------------------------------------------------
KNOWN
------------------------------------------------------------------------------

Development build. Not notarised. One automated test is deliberately failing
(a loudness-matching criterion on plucked material) - it does not affect what
you hear. 27 factory presets; more to come after a listening pass.
