# NYC & Hawkeye Research Notes

Research for a solo UE5.8 third-person fan game set in the NYC of Disney+'s *Hawkeye*
(2021). Part 1 is the show's geography and canon; Part 2 is how to build a realistic
NYC in Unreal as a solo dev.

## Part 1: The Show's New York

### Location table

| In-show location | Real filming location | Meant-to-be NYC neighbourhood | Notes / coords |
|---|---|---|---|
| Kate's apartment (above "Herman's Hearty Slice" pizza shop) | Built on Broad Street SW, Atlanta, GA (not a real NYC storefront) | Implied Lower Manhattan/Tribeca-ish walk-up | Unverified exact real-world stand-in; Atlanta set dressed with NYC signage [Geek Trippers](https://geektrippers.com/hawkeye-filming-locations/) |
| Eleanor Bishop's penthouse / Bishop Security offices | Interior stages, Atlanta | Upper East Side (Park Ave-coded) | Doorman-lobby scenes shot on Atlanta soundstages [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Charity auction / black-market auction beneath it | Set built to evoke "corner of Park Ave and 68th St" | Upper East Side | Address is a production placement, not a real venue [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Armand Duquesne's brownstone (murder scene) | Brownstone set, New York/Atlanta mix | Unverified — coded as UES or Gramercy brownstone row | [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Clint's hotel | Lotte New York Palace Hotel used for some hotel material; Peninsula Hotel also referenced | Midtown East (Madison Ave near 50th St) | Real NYC hotel used for exteriors during Dec 2020 NYC shoot [Newsweek](https://www.newsweek.com/hawkeye-filming-locations-set-mcu-timeline-1653289), [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Tracksuit Mafia turf / auto shop confrontations | Atlanta soundstage/backlot | Coded as an outer-borough or Lower Manhattan industrial block | Unverified real NYC analogue [Wikipedia](https://en.wikipedia.org/wiki/Hawkeye_(miniseries)) |
| Subway escape | Chambers Street station used as a reference/filming point; Hoyt–Schermerhorn (Downtown Brooklyn) used during real NYC unit shoot | Lower Manhattan / Downtown Brooklyn transit | [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/), [Newsweek](https://www.newsweek.com/hawkeye-filming-locations-set-mcu-timeline-1653289) |
| LARP park | Central Park (NYC unit shoot) | Central Park | Real NYC location used for the LARPers' scenes [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Christmas tree lot ("Rosie's") | Practical dressing, Brooklyn | Brooklyn | [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Rockefeller Center finale (tree, ice rink, plaza entrance) | Real Rockefeller Plaza entrance used for some shots; the ice-rink fight and tree pull were shot on a purpose-built Georgia set (Tyler Perry Studios) | Midtown, Rockefeller Center, ~40.7587°N 73.9787°W | [Fantrippers](https://www.fantrippers.com/en/5-filming-locations-of-hawkeye-disney/), [Sceen-it](https://www.sceen-it.com/sceen/5608/Hawkeye/The-Rink-At-Rockefeller-Center) |
| FAO Schwarz at Rockefeller Center | Set/practical location tie-in | Rockefeller Center, Midtown | [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Times Square | Referenced/backdrop | Midtown | [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/) |
| Washington Square Park / NYU area, Hell's Kitchen, East Village | Real NYC locations used during the Dec 2020 principal-unit shoot | Greenwich Village / Hell's Kitchen / East Village | [Wikipedia](https://en.wikipedia.org/wiki/Hawkeye_(miniseries)) |
| Maya Lopez's territory / Echo material | Not detailed in available sources for season 1 proper (expanded in *Echo*, 2024) | Hell's Kitchen-coded (comics-standard turf) | Unverified for this show specifically |

**Filming split, in short:** the December 2020 unit shoot used real NYC — Central Park,
Washington Square Park, Hell's Kitchen, the East Village, and Hoyt–Schermerhorn in
Downtown Brooklyn — for exteriors and texture. Almost everything with dialogue and
stunts (Kate's apartment block, the Bishop penthouse, the auction, the Rockefeller
Center ice rink/tree fight) was built on stages and backlots at Trilith Studios and
Tyler Perry Studios in the Atlanta area, plus additional unit work in downtown Canton,
GA. Reshoots happened in Toronto. [Newsweek](https://www.newsweek.com/hawkeye-filming-locations-set-mcu-timeline-1653289), [Backstage](https://www.backstage.com/magazine/article/atlanta-whats-filming-disney-plus-hawkeye-series-72794/), [MCU Location Scout](https://mculocationscout.com/tv-shows/hawkeye-season-1/)

### Time of year and weather

The show is explicitly set during Christmas week, one year after *Avengers: Endgame*
(so December 2024 in-universe). The production has said the holiday setting was
treated "like a character in itself" — snow, string lights, tree lots, ice rinks, and
Christmas markets recur throughout. [Wikipedia](https://en.wikipedia.org/wiki/Hawkeye_(miniseries))

### End-of-series character status

| Character | Status at end of season 1 |
|---|---|
| Kate Bishop | Reconciled with Clint as his protégé; turns her own mother in to the authorities. [The Ringer](https://www.theringer.com/2021/12/22/marvel/hawkeye-finale-kate-bishop-black-widow-kingpin-lucky) |
| Clint Barton | Convinces Yelena he didn't kill Natasha by choice; returns to the Barton family farm for Christmas with Kate and Lucky. [Inverse](https://www.inverse.com/entertainment/hawkeye-ending-explained-kate-bishop-avengers) |
| Yelena Belova | Stands down against Clint after hearing him out; leaves NYC. [The Ringer](https://www.theringer.com/2021/12/22/marvel/hawkeye-finale-kate-bishop-black-widow-kingpin-lucky) |
| Eleanor Bishop | Revealed to have worked for Kingpin, had Armand Duquesne killed and framed Jack; arrested after Kate turns her in. [Screen Rant](https://screenrant.com/hawkeye-characters-marvel-future-movies/) |
| Kingpin (Wilson Fisk) | Shot at point-blank by Maya Lopez in the finale; not shown to die on-screen — fate left ambiguous/unverified as of the show itself. [Collider](https://collider.com/hawkeye-ending-explained-kingpin-watch/) |
| Maya Lopez | Learns Fisk ordered her father's death; shoots him in apparent revenge, spun off into her own 2024 series *Echo*. [Screen Rant](https://screenrant.com/hawkeye-characters-marvel-future-movies/) |
| Jack Duquesne | Cleared of suspicion, fights off the Tracksuit Mafia, ends the season on good terms with Kate's LARPer allies. [Screen Rant](https://screenrant.com/hawkeye-characters-marvel-future-movies/) |
| Lucky ("Pizza Dog") | Formally adopted/named "Lucky"; goes home with Clint and Kate to the Barton farm. [The Ringer](https://www.theringer.com/2021/12/22/marvel/hawkeye-finale-kate-bishop-black-widow-kingpin-lucky) |
| Grills | Unverified — not covered in the sources checked for this pass; treat his fate as open unless confirmed elsewhere. |

### Barney Barton / Trickshot

- Charles "Barney" Barton first appeared in *Avengers* vol. 1 #64 (May 1969), created by
  Roy Thomas and Gene Colan, as Clint's older brother. [Wikipedia](https://en.wikipedia.org/wiki/Barney_Barton)
- Comics backstory: the Barton brothers grew up in an abusive household in Waverly,
  Iowa; after their parents died they ran away and worked as roustabouts for the Carson
  Carnival of Traveling Wonders, where Clint trained under the Swordsman and Trick Shot.
  Barney later drifted into crime and a double life as an FBI informant. [Wikipedia](https://en.wikipedia.org/wiki/Barney_Barton)
- **Two Trick Shots exist in the comics:** Buck Chisholm was the original archer villain
  using the "Trick Shot" name (trained young Clint and Barney at the carnival); Barney
  Barton later took up the codename himself, rebranded as "Trickshot," starting in
  *Hawkeye: Blindspot* #1 (2011, Jim McCann/Paco Diaz), and became a recurring cast
  member of *Dark Avengers* from issue #175. [Wikipedia](https://en.wikipedia.org/wiki/Barney_Barton), [Comic Vine](https://comicvine.gamespot.com/trickshot/4005-70149/)
- Powers/style: no superpowers — an expert marksman and gadgeteer archer like Clint,
  using trick arrows and hand-to-hand combat; his character arc is defined by sibling
  rivalry with Hawkeye rather than a unique power set. [Marvel](https://www.marvel.com/characters/trickshot)
- MCU status as of this research (September 2026): Barney/Trickshot has **not**
  appeared in the MCU. There were unconfirmed reports in early 2025 of an offer to an
  A-list actor for a recurring Barney Barton role, reportedly tied to a future
  *Hawkeye* season, but this is rumor, not confirmed casting — treat as unverified.
  [IMDb news](https://www.imdb.com/news/ni65051707/), [LRM Online](https://lrmonline.com/news/marvel-offer-a-list-actor-recurring-role-as-barney-barton-aka-trickshot-barside-buzz/)
- Best comics to read for the character: *Hawkeye: Blindspot* #1 (2011) for his
  Trickshot debut, and the *Dark Avengers* run starting at issue #175 for him as a
  recurring cast member. [Wikipedia](https://en.wikipedia.org/wiki/Barney_Barton)

### Tone reference: Fraction/Aja *Hawkeye* (2012–2015)

- Written by Matt Fraction, drawn primarily by David Aja, published August 2012–July
  2015, 22 issues, collected into three trades: *My Life as a Weapon*, *Little Hits*,
  and *L.A. Woman*. [Marvel](https://www.marvel.com/comics/series/16309/hawkeye_2012_-_2015), [TV Tropes](https://tvtropes.org/pmwiki/pmwiki.php/ComicBook/Hawkeye2012)
- Structure: loose, mostly standalone "what Clint/Kate do when they're not Avengers"
  issues rather than one continuous arc, told out of chronological order with
  flash-forwards and flash-backs. [TV Tropes](https://tvtropes.org/pmwiki/pmwiki.php/ComicBook/Hawkeye2012)
- Visual style: Aja's minimalist, high-contrast linework with a restricted, purple-and
  -flat-color palette; avant-garde panel/page layouts (famous "Pizza Dog" issue told
  largely from the dog's POV via icons). [CBR](https://www.cbr.com/hawkeye-matt-fraction-david-aja-visual-storytelling-marvel/), [Hypercritic](https://hypercritic.org/collection/hawkeye-2012-2015-when-an-artistic-style-becomes-storytelling)
- The show adapted this run's tone directly: street-level, apartment-building/borough
  scale storytelling instead of cosmic stakes; Pizza Dog (renamed Lucky) as a
  recurring character; Kate Bishop introduced as the "regular person" Hawkeye; and the
  general "this is what off-duty Avengers business looks like" register carries over
  from page to screen (own synthesis based on sourced plot/character facts above —
  unverified as a direct producer statement).

## Part 2: Getting Real New York into Unreal

### Data source comparison

| Source | What you get | Format | Heights? | Licence | Fit for a solo dev |
|---|---|---|---|---|---|
| OSM via Overpass API | Building footprints (polygons) + tags | Overpass QL / JSON / OSM XML, convert to GeoJSON | Yes, where tagged: `height` (meters) and/or `building:levels` (× ~3 m/floor); Manhattan's footprints were bulk-imported from NYC's 2013–2014 DOITT survey with `height` and `nycdoitt:bin` tags, so **coverage in Manhattan is unusually good** compared to most cities | ODbL (attribution + share-alike on the *data*, not on your game) | Best free source for citywide footprints + rough heights |
| NYC Open Data "3-D Building Model" | Photogrammetry-derived roof/facade/footprint linework per building, correct real-world elevation, from a 2014 aerial survey | Rhino/Grasshopper-Meerkat-oriented linework + surfaces, split into building components across ~14 secondary layers | Implicitly yes (correct elevation per building) but not a simple flat "height" column — must be derived from geometry | NYC Open Data default (public domain-style, free reuse) | Higher fidelity than OSM but heavier to parse; good for a handful of hero buildings, not a whole borough |
| NYC Open Data "Building Footprints" (BYTES of the BIG APPLE) | Simple footprint polygons | Shapefile/GeoJSON | No height column by default (separate from the 3-D model) | NYC Open Data | Good simple footprint source, pair with LiDAR/3-D model or OSM heights |
| Cesium for Unreal + Google Photorealistic 3D Tiles | Full photogrammetry mesh of real Manhattan, streamed at runtime, correct texture/geometry down to street level | Runtime-streamed 3D Tiles (not exportable assets) | N/A — full mesh | Google's Tile API is metered per "root tile" request (first 1,000/month free per Cesium ion plan, then billed); usable in a shipped product but at ongoing per-player cost, and there have been reported bugs with tiles not appearing in Shipping builds | Excellent as **reference/greybox validation**, risky as a **shipped** dependency for a solo dev (recurring cost per player session, packaging bugs) |
| Blosm (formerly blender-osm) | One-click OSM building import into Blender with height/levels-driven extrusion, plus terrain and Google 3D Tiles import; free tier + paid "Premium" for materials/UV/textures | Blender scene → FBX export | Yes, driven by OSM height/level tags | Commercial add-on (free + premium tiers), your exported geometry is yours | Best practical bridge from OSM data to Unreal-importable meshes |
| Fab (Epic marketplace) NYC-themed packs | Modular brownstone kits, fire escapes, NYC street props, generic "city block" kits | Unreal-ready (`.uasset`) | N/A (hand-modeled) | Per-asset Fab EULA | Good for hand-dressing hero blocks once the greybox is right; no single dominant "official NYC" pack found in this search — evaluate specific listings on fab.com directly since search results here only confirmed Fab exists as the merged Epic marketplace, not specific pack titles. [Fab](https://www.fab.com/) |
| Google Street View | Photo reference per address, no export | Web viewer | N/A | Google ToS — reference only, no reuse of imagery in-game | Use only as artist reference |
| Mapillary | Crowd-sourced street-level photos with a documented API (images, sequences, detections) | REST API, MapillaryJS viewer, vector tiles | N/A | Free to use, CC-licensed imagery per contributor terms | Good free alternative/supplement to Street View for texture reference, and it's actually queryable/downloadable unlike Street View. [Mapillary Developer](https://www.mapillary.com/developer), [Mapillary API docs](https://www.mapillary.com/developer/api-documentation) |
| NYC DOT camera feeds | Live traffic-cam stills across NYC intersections | Public webcam snapshot API at `webcams.nyctmc.org/api/cameras/`; full data-feed access requires a signed agreement with NYC DOT TMC | N/A | Public snapshots free; bulk feed access requires contacting `TMCDOT@dot.nyc.gov` | Useful for current street conditions/atmosphere reference only |

### Recommended pipeline

1. **Footprints + heights (bulk, automated):** Pull OSM building polygons for the target
   area via Overpass, keeping `height`/`building:levels` tags — Manhattan's OSM data is
   unusually reliable here because it originates from NYC's own DOITT survey. [OSM Wiki](https://wiki.openstreetmap.org/wiki/Overpass_API/Overpass_API_by_Example), [Carpentries Overpass tutorial](https://carpentries-incubator.github.io/r-geospatial-urban/instructor/18-import-and-visualise-osm-data.html)
2. **Greybox generation (scripted):** Import that OSM data into Blender with Blosm,
   which extrudes footprints to their tagged height/level count automatically, then
   batch-export FBX per city block for Unreal import — this is the "generated
   greybox," not hand-modeled. [Blosm Wiki](https://github.com/vvoovv/blosm/wiki/Premium-Version), [Blosm docs](https://blendervisualinvestigation.com/knowledge-base/docs/addons/blosm/)
3. **Reference only, never shipped:** Use Cesium for Unreal + Google Photorealistic 3D
   Tiles in editor (or a throwaway reference level) to sanity-check massing, skyline
   silhouette, and street proportions against the real thing, plus Street View and
   Mapillary for facade/material photo reference. Don't wire Google 3D Tiles into the
   shipped build — the per-root-tile billing and reported Shipping-build tile issues
   make it a poor fit for a solo dev's finished product. [Cesium Community: cost](https://community.cesium.com/t/google-tiles-api-no-longer-required-how-does-pricing-work/28017), [Cesium Community: shipping bug](https://community.cesium.com/t/missing-3d-tiles-in-shipping-build/47098)
4. **Hand-dress hero blocks:** Once the citywide greybox is in and playable at correct
   scale, hand-replace/dress the handful of blocks that matter for a given mission
   (Kate's block, Rockefeller Plaza) with Fab kit assets and custom materials; leave
   everything else as textured greybox for skyline/backdrop.

### Example Overpass query (1 km box, Rockefeller Center area)

Rockefeller Center is at approximately 40.7587°N, 73.9787°W. A ~1 km box around it,
buildings with height data:

```
[out:json][timeout:25];
(
  way["building"]["height"](40.7537,-73.9847,40.7637,-73.9727);
  way["building"]["building:levels"](40.7537,-73.9847,40.7637,-73.9727);
);
out body geom;
```

(Bounding box order for Overpass is `south,west,north,east`.) [OSM Help](https://help.openstreetmap.org/questions/46867/how-to-use-overpass-api-for-specific-boundingbox), [Overpass QL wiki](https://wiki.openstreetmap.org/wiki/Overpass_API/Overpass_QL)

### Projection math: lat/lon → Unreal centimetres

OSM data is in WGS84 lat/lon (degrees). Unreal wants a flat, metric, left-handed
Cartesian space in centimetres. Two workable approaches:

- **UTM Zone 18N** (Manhattan's UTM zone): reproject WGS84 → UTM 18N (EPSG:32618) to
  get planar X/Y in metres with low distortion over a borough-sized area, pick one
  point in the AOI as your world origin, subtract it from every coordinate, then
  multiply by 100 to get Unreal centimetres (`UnrealX_cm = (utmX - originX) * 100`,
  `UnrealY_cm = (utmY - originY) * 100`, note Unreal's Y often needs sign-flipping
  depending on your import convention).
- **Local tangent plane / equirectangular approximation** (simpler, fine at
  sub-5 km scale): pick an origin `(lat0, lon0)`, then approximate
  `x_m = (lon - lon0) * 111320 * cos(lat0_in_radians)` and
  `y_m = (lat - lat0) * 110540`, then scale to centimetres the same way. This avoids a
  full projection library and is accurate enough for a several-block playable area;
  switch to true UTM if the map grows past a few kilometres.

This is standard geodesy/GIS practice, not something specific to a citable source —
Blosm and most OSM-to-engine pipelines perform an equivalent local-tangent-plane or UTM
conversion internally before extrusion. [Blosm Wiki](https://github.com/vvoovv/blosm/wiki/Premium-Version)

### Scale sanity check

| Element | Real-world size | Source |
|---|---|---|
| Standard Manhattan block (between avenues, north-south) | ~80 m × 274 m (264 ft × 900 ft) | [Quora/city-block refs](https://www.quora.com/What-are-the-dimensions-of-a-NYC-block) |
| Avenue-to-avenue spacing | ~200 ft (~61 m) block width plus a 60 ft (~18 m) cross street | Commissioners' Plan of 1811 sourcing |
| Numbered cross-streets width | 60 ft (~18 m) | Commissioners' Plan of 1811 |
| Major avenues width | 100 ft (~30 m) | Commissioners' Plan of 1811 |
| Typical brownstone height | 3–4 storeys, roughly 12–15 m to the cornice (not independently re-verified in this pass — treat as unverified ballpark, cross-check against OSM `building:levels` for your specific target blocks) | Unverified |

Use the block and street numbers above as hard constraints when laying out a generated
greybox district — if the Overpass/Blosm output doesn't land close to 80 m × 274 m
blocks with ~18 m streets, the origin/scale conversion is wrong before anything else is
debugged.

## Sources

- [Hawkeye (miniseries) — Wikipedia](https://en.wikipedia.org/wiki/Hawkeye_(miniseries))
- [MCU Location Scout — Hawkeye Season 1](https://mculocationscout.com/tv-shows/hawkeye-season-1/)
- [Newsweek — Hawkeye filming locations and MCU timeline](https://www.newsweek.com/hawkeye-filming-locations-set-mcu-timeline-1653289)
- [Backstage — Atlanta What's Filming: Hawkeye](https://www.backstage.com/magazine/article/atlanta-whats-filming-disney-plus-hawkeye-series-72794/)
- [Fantrippers — 5 filming locations of Hawkeye](https://www.fantrippers.com/en/5-filming-locations-of-hawkeye-disney/)
- [Geek Trippers — Full list of Hawkeye filming locations](https://geektrippers.com/hawkeye-filming-locations/)
- [Sceen-it — The Rink at Rockefeller Center](https://www.sceen-it.com/sceen/5608/Hawkeye/The-Rink-At-Rockefeller-Center)
- [The Ringer — Hawkeye finale recap](https://www.theringer.com/2021/12/22/marvel/hawkeye-finale-kate-bishop-black-widow-kingpin-lucky)
- [Inverse — Hawkeye ending explained](https://www.inverse.com/entertainment/hawkeye-ending-explained-kate-bishop-avengers)
- [Collider — Hawkeye ending explained, Kingpin and the watch](https://collider.com/hawkeye-ending-explained-kingpin-watch/)
- [Screen Rant — Every Hawkeye character's future explained](https://screenrant.com/hawkeye-characters-marvel-future-movies/)
- [Wikipedia — Barney Barton](https://en.wikipedia.org/wiki/Barney_Barton)
- [Comic Vine — Trickshot](https://comicvine.gamespot.com/trickshot/4005-70149/)
- [Marvel.com — Trickshot](https://www.marvel.com/characters/trickshot)
- [IMDb news — Barney Barton casting rumor](https://www.imdb.com/news/ni65051707/)
- [LRM Online — Barney Barton/Trickshot casting rumor](https://lrmonline.com/news/marvel-offer-a-list-actor-recurring-role-as-barney-barton-aka-trickshot-barside-buzz/)
- [Marvel.com — Hawkeye (2012-2015) series](https://www.marvel.com/comics/series/16309/hawkeye_2012_-_2015)
- [TV Tropes — Hawkeye (2012) comic](https://tvtropes.org/pmwiki/pmwiki.php/ComicBook/Hawkeye2012)
- [CBR — Hawkeye visual storytelling](https://www.cbr.com/hawkeye-matt-fraction-david-aja-visual-storytelling-marvel/)
- [Hypercritic — Hawkeye artistic style](https://hypercritic.org/collection/hawkeye-2012-2015-when-an-artistic-style-becomes-storytelling)
- [OpenStreetMap Wiki — Overpass API by Example](https://wiki.openstreetmap.org/wiki/Overpass_API/Overpass_API_by_Example)
- [OpenStreetMap Wiki — Overpass QL](https://wiki.openstreetmap.org/wiki/Overpass_API/Overpass_QL)
- [OSM Help — Overpass bounding box](https://help.openstreetmap.org/questions/46867/how-to-use-overpass-api-for-specific-boundingbox)
- [Carpentries Incubator — Import and visualise OSM data](https://carpentries-incubator.github.io/r-geospatial-urban/instructor/18-import-and-visualise-osm-data.html)
- [NYC Open Data — 3-D Building Model](https://data.cityofnewyork.us/widgets/tnru-abg2)
- [NYC Open Data — Building Footprints](https://data.cityofnewyork.us/City-Government/Building-Footprints/5zhs-2jue)
- [nyc-geo-metadata GitHub — Building Footprints metadata](https://github.com/CityOfNewYork/nyc-geo-metadata/blob/main/Metadata/Metadata_BuildingFootprints.md)
- [Cesium — Photorealistic 3D Tiles for Unreal](https://cesium.com/learn/unreal/unreal-photorealistic-3d-tiles/)
- [Cesium Community — Google 3D Tiles cost](https://community.cesium.com/t/google-tiles-api-no-longer-required-how-does-pricing-work/28017)
- [Cesium Community — Missing 3D Tiles in Shipping build](https://community.cesium.com/t/missing-3d-tiles-in-shipping-build/47098)
- [Blosm GitHub Wiki — Premium Version](https://github.com/vvoovv/blosm/wiki/Premium-Version)
- [Blender Visual Investigation — Blosm docs](https://blendervisualinvestigation.com/knowledge-base/docs/addons/blosm/)
- [Fab (website) — Wikipedia](https://en.wikipedia.org/wiki/Fab_(website))
- [Fab.com](https://www.fab.com/)
- [Mapillary Developer portal](https://www.mapillary.com/developer)
- [Mapillary API documentation](https://www.mapillary.com/developer/api-documentation)
- [GitHub — joerodrig/nyc_dot_cctv](https://github.com/joerodrig/nyc_dot_cctv)
- [511NY Developer Help](https://511ny.org/developers/help)
- [Quora — NYC block dimensions](https://www.quora.com/What-are-the-dimensions-of-a-NYC-block)
