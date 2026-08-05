FOUR COLOR — Windows install
Multiband Colour & Saturation · by Gussa Naaman · v0.1.0 · x64

------------------------------------------------------------------------------
INSTALL
------------------------------------------------------------------------------

  1. EXTRACT this ZIP to a real folder first.
     Do not run the installer from inside the ZIP viewer - Windows copies the
     .bat somewhere on its own and leaves the plugin behind, and then there is
     nothing to install.

  2. Close Cubase (or whatever host you use).
     Windows will not replace a plugin a running host has open. This is the
     usual reason an update appears to do nothing.

  3. RIGHT-CLICK  INSTALL-FOUR-COLOR.bat  ->  "Run as administrator".

  It installs:

    FourColor.vst3  ->  C:\Program Files\Common Files\VST3
    FourColor.exe   ->  C:\Program Files\Naaman\FOUR COLOR   (standalone)

  4. Start Cubase, then Studio > VST Plug-in Manager > Update.
     FOUR COLOR appears under Naaman, category Distortion.

------------------------------------------------------------------------------
IF SOMETHING GOES WRONG
------------------------------------------------------------------------------

The installer checks every step and stops with a reason rather than claiming
success. If it stops, the window itself is the diagnosis - send me a
screenshot of it. The path it prints is the clue.

To start clean:  right-click UNINSTALL-FOUR-COLOR.bat -> "Run as administrator".
That clears every plausible location and both install shapes, and says what it
found in each.

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

------------------------------------------------------------------------------
WORTH CHECKING ON THIS BUILD
------------------------------------------------------------------------------

  * Reported latency in Cubase should be 65 samples at EVERY Quality setting.
    That is deliberate - it is what makes Quality safe to change mid-song.
  * Move a crossover handle while playing: no zipper, no click.
  * Switch Quality while playing, all four: no dropout.
  * Save the project, close Cubase, reopen: every parameter returns.

------------------------------------------------------------------------------
KNOWN
------------------------------------------------------------------------------

Development build, unsigned. 27 factory presets; more after a listening pass.
One automated test is deliberately left failing (a loudness-matching criterion
on plucked material) - it does not affect what you hear.
