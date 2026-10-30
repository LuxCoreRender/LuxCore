Prague Sky Model, vendored from https://github.com/PetrVevoda/pragueskymodel
(revision 2385c91, Apache-2.0, see LICENSE).

Used by the LuxCoreRender "sky3" light source (slg/lights/sky3light.*) to
evaluate sky radiance, sun radiance and transmittance from the full
SkyModelDataset.dat dataset.

The files here are the original sources with a single deviation:

- The global free function `lerp()` has been renamed to `lerpPrague()`
  (declaration and all call sites). The original name collides with C++20
  `std::lerp()` in translation units that have `using namespace std;` active
  (as all LuxCoreRender ones do), making the call ambiguous and the
  declarations conflicting. Everything else is unmodified.
