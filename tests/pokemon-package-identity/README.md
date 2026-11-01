The fixtures are the generated manifest and 1,848-byte Windows resource index
from the verified .749 package. They contain package/resource metadata only;
no Pikachu model, game executable, game image or signing key is included.

The Windows job reindexes these exact candidates into `DusklightPokemonPreview`.
Private final packaging requires the new build's original `resources.pri` hash
to match this input exactly. If resources change, this fixture must be refreshed
and the real Windows reindexing check repeated before packaging can proceed.

The separate identity uses its own package family and PhoneProductId. The
executable name and entry point remain unchanged. Do not deliver an installer
whose package Name is `TwilightPrincessRandomizer` as a separate preview app.
