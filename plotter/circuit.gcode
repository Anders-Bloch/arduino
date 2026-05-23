; Cat drawing — 50 x 50 mm
; Supported commands: G0 (pen up / rapid), G1 (pen down / draw), G90, M2
G90

; === Body (oval, centre X25 Y11, semi-x 12 mm, semi-y 10 mm) ===
G0 X37.0 Y11.0
G1 X36.1 Y14.8
G1 X33.5 Y18.1
G1 X29.6 Y20.2
G1 X25.0 Y21.0
G1 X20.4 Y20.2
G1 X16.5 Y18.1
G1 X13.9 Y14.8
G1 X13.0 Y11.0
G1 X13.9 Y7.2
G1 X16.5 Y3.9
G1 X20.4 Y1.8
G1 X25.0 Y1.0
G1 X29.6 Y1.8
G1 X33.5 Y3.9
G1 X36.1 Y7.2
G1 X37.0 Y11.0

; === Head (circle, centre X25 Y30, radius 11 mm) ===
G0 X36.0 Y30.0
G1 X35.2 Y34.2
G1 X32.8 Y37.8
G1 X29.2 Y40.2
G1 X25.0 Y41.0
G1 X20.8 Y40.2
G1 X17.2 Y37.8
G1 X14.8 Y34.2
G1 X14.0 Y30.0
G1 X14.8 Y25.8
G1 X17.2 Y22.2
G1 X20.8 Y19.8
G1 X25.0 Y19.0
G1 X29.2 Y19.8
G1 X32.8 Y22.2
G1 X35.2 Y25.8
G1 X36.0 Y30.0

; === Left ear (triangle, attached to head circle points) ===
G0 X17.2 Y37.8
G1 X13.5 Y47.0
G1 X20.8 Y40.2
G1 X17.2 Y37.8

; === Right ear (triangle, attached to head circle points) ===
G0 X29.2 Y40.2
G1 X36.5 Y47.0
G1 X32.8 Y37.8
G1 X29.2 Y40.2

; === Left eye (circle, centre X20 Y31, radius 2 mm) ===
G0 X22.0 Y31.0
G1 X21.4 Y32.4
G1 X20.0 Y33.0
G1 X18.6 Y32.4
G1 X18.0 Y31.0
G1 X18.6 Y29.6
G1 X20.0 Y29.0
G1 X21.4 Y29.6
G1 X22.0 Y31.0

; === Right eye (circle, centre X30 Y31, radius 2 mm) ===
G0 X32.0 Y31.0
G1 X31.4 Y32.4
G1 X30.0 Y33.0
G1 X28.6 Y32.4
G1 X28.0 Y31.0
G1 X28.6 Y29.6
G1 X30.0 Y29.0
G1 X31.4 Y29.6
G1 X32.0 Y31.0

; === Nose (small triangle) ===
G0 X24.0 Y28.5
G1 X26.0 Y28.5
G1 X25.0 Y27.0
G1 X24.0 Y28.5

; === Mouth (Y-shape from nose tip) ===
G0 X25.0 Y27.0
G1 X25.0 Y26.0
G1 X22.5 Y24.5
G0 X25.0 Y26.0
G1 X27.5 Y24.5

; === Whiskers — left ===
G0 X23.5 Y28.5
G1 X8.0 Y30.0
G0 X23.5 Y27.5
G1 X8.0 Y27.5
G0 X23.5 Y26.5
G1 X8.0 Y25.0

; === Whiskers — right ===
G0 X26.5 Y28.5
G1 X42.0 Y30.0
G0 X26.5 Y27.5
G1 X42.0 Y27.5
G0 X26.5 Y26.5
G1 X42.0 Y25.0

; === Tail (sweeps from body right side and curls up) ===
G0 X36.1 Y7.2
G1 X41.0 Y4.0
G1 X45.0 Y2.0
G1 X47.0 Y4.5
G1 X45.0 Y8.5
G1 X40.0 Y11.5
G1 X37.0 Y11.0

; === Return to home ===
G0 X0.0 Y0.0
M2
