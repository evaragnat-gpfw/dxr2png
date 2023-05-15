# Usage

```
dxr2png GOPR3241.dxr    # will produce GOPR3241.png with all planes

dxr2png GOPR3241.dxr Gr # will produce GOPR3241-Gr.png with only Gr plane

dxr2png --no-binning GOPR3241.dxr Gr # will produce GOPR3241-Gr.png with only Gr plane
                                     # and all other planes black
```

# Supported DXR formats

        precision  sample    packed   status
Bayer0  12-bit     unsigned  yes      OK
Bayer0  12-bit     16-bit    no       OK
Bayer0  10-bit     unsigned  yes      TODO

# Tests

Extraction and correctness can be easily tested with DXR from sensor pattern.
Samples are coming from :
	Still with sensor pattern	: 12-bit packed
	HDR still with sensor pattern   : 12-bit unpacked (in 16-bit samples)
	??? fro Vincent D.		: 10-bit packed
