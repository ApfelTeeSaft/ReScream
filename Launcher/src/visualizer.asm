include common.inc
include globals.inc

PUBLIC DrawVisualizer, DestroyVisualizerResources

SCOPE_GREEN_GHOST_2     EQU 00101E08h
SCOPE_GREEN_GHOST_1     EQU 001B3D10h
SCOPE_GREEN_FAR         EQU 00225312h
SCOPE_GREEN_SIDE        EQU 003C9B24h
SCOPE_GREEN_NEAR        EQU 005BEA38h
SCOPE_GREEN_BEAM        EQU 008CFF72h

SCOPE_GRID_MINOR        EQU 000B1808h
SCOPE_GRID_MAJOR        EQU 00142E0Fh
SCOPE_GRID_AXIS         EQU 00225312h
GRID_MINOR_STEP         EQU 40
GRID_MAJOR_STEP         EQU 160

PHASE_MASK              EQU 0000FFFFh
PHASE_QUARTER           EQU 16384
PHASE_FULL              EQU 65536
ROTATION_PERIOD_MS      EQU 9000

MODEL_CENTER_X          EQU 38
MODEL_CENTER_Y          EQU 7
MODEL_DEPTH             EQU 36
MODEL_SCALE             EQU 10
FOCAL_LENGTH            EQU 1120
MIN_PERSPECTIVE_DENOM   EQU 320

VEC_ENTRY_BYTES         EQU 16

.data
sineTable dd 0,25,50,75,100,125,150,175
          dd 200,224,249,273,297,321,345,369
          dd 392,415,438,460,483,505,526,548
          dd 569,590,610,630,650,669,688,706
          dd 724,742,759,775,792,807,822,837
          dd 851,865,878,891,903,915,926,936
          dd 946,955,964,972,980,987,993,999
          dd 1004,1009,1013,1016,1019,1021,1023,1024
          dd 1024,1024,1023,1021,1019,1016,1013,1009
          dd 1004,999,993,987,980,972,964,955
          dd 946,936,926,915,903,891,878,865
          dd 851,837,822,807,792,775,759,742
          dd 724,706,688,669,650,630,610,590
          dd 569,548,526,505,483,460,438,415
          dd 392,369,345,321,297,273,249,224
          dd 200,175,150,125,100,75,50,25
          dd 0,-25,-50,-75,-100,-125,-150,-175
          dd -200,-224,-249,-273,-297,-321,-345,-369
          dd -392,-415,-438,-460,-483,-505,-526,-548
          dd -569,-590,-610,-630,-650,-669,-688,-706
          dd -724,-742,-759,-775,-792,-807,-822,-837
          dd -851,-865,-878,-891,-903,-915,-926,-936
          dd -946,-955,-964,-972,-980,-987,-993,-999
          dd -1004,-1009,-1013,-1016,-1019,-1021,-1023,-1024
          dd -1024,-1024,-1023,-1021,-1019,-1016,-1013,-1009
          dd -1004,-999,-993,-987,-980,-972,-964,-955
          dd -946,-936,-926,-915,-903,-891,-878,-865
          dd -851,-837,-822,-807,-792,-775,-759,-742
          dd -724,-706,-688,-669,-650,-630,-610,-590
          dd -569,-548,-526,-505,-483,-460,-438,-415
          dd -392,-369,-345,-321,-297,-273,-249,-224
          dd -200,-175,-150,-125,-100,-75,-50,-25

gScopeDC                dd 0
gScopeBitmap            dd 0
gScopeOldBitmap         dd 0
gScopeWidth             dd 0
gScopeHeight            dd 0
gGridMinorPen           dd 0
gGridMajorPen           dd 0
gGridAxisPen            dd 0

vectorSegments LABEL DWORD

; S (x 0..10)
    dd 10,1,  8,0
    dd 8,0,   2,0
    dd 2,0,   0,2
    dd 0,2,   0,5
    dd 0,5,   2,7
    dd 2,7,   8,7
    dd 8,7,   10,9
    dd 10,9,  10,12
    dd 10,12, 8,14
    dd 8,14,  2,14
    dd 2,14,  0,13

; C (x 13..23)
    dd 23,2,  21,0
    dd 21,0,  15,0
    dd 15,0,  13,2
    dd 13,2,  13,12
    dd 13,12, 15,14
    dd 15,14, 21,14
    dd 21,14, 23,12

; R (x 26..36)
    dd 26,14, 26,0
    dd 26,0,  33,0
    dd 33,0,  36,2
    dd 36,2,  36,6
    dd 36,6,  33,7
    dd 33,7,  26,7
    dd 31,7,  36,14

; E (x 39..49)
    dd 39,0,  39,14
    dd 39,0,  49,0
    dd 39,7,  47,7
    dd 39,14, 49,14

; 4 (x 52..62)
    dd 60,0,  52,9
    dd 52,9,  62,9
    dd 60,0,  60,14

; M (x 65..76)
    dd 65,14, 65,0
    dd 65,0,  70,7
    dd 70,7,  76,0
    dd 76,0,  76,14

VECTOR_SEGMENT_COUNT EQU ($ - vectorSegments) / VEC_ENTRY_BYTES


.code

DestroyVisualizerResources PROC
    cmp gScopeDC, 0
    je DestroyGridPens

    cmp gScopeBitmap, 0
    je DestroyScopeDC
    cmp gScopeOldBitmap, 0
    je DeleteScopeBitmap
    invoke SelectObject, gScopeDC, gScopeOldBitmap
DeleteScopeBitmap:
    invoke DeleteObject, gScopeBitmap

DestroyScopeDC:
    invoke DeleteDC, gScopeDC
    mov gScopeDC, 0
    mov gScopeBitmap, 0
    mov gScopeOldBitmap, 0
    mov gScopeWidth, 0
    mov gScopeHeight, 0

DestroyGridPens:
    cmp gGridMinorPen, 0
    je DestroyMajorPen
    invoke DeleteObject, gGridMinorPen
    mov gGridMinorPen, 0
DestroyMajorPen:
    cmp gGridMajorPen, 0
    je DestroyAxisPen
    invoke DeleteObject, gGridMajorPen
    mov gGridMajorPen, 0
DestroyAxisPen:
    cmp gGridAxisPen, 0
    je DestroyScopeDone
    invoke DeleteObject, gGridAxisPen
    mov gGridAxisPen, 0
DestroyScopeDone:
    ret
DestroyVisualizerResources ENDP

EnsureGridPens PROC
    cmp gGridMinorPen, 0
    jne GridPensReady
    invoke CreatePen, PS_SOLID, 1, SCOPE_GRID_MINOR
    mov gGridMinorPen, eax
    invoke CreatePen, PS_SOLID, 2, SCOPE_GRID_MAJOR
    mov gGridMajorPen, eax
    invoke CreatePen, PS_SOLID, 2, SCOPE_GRID_AXIS
    mov gGridAxisPen, eax
GridPensReady:
    ret
EnsureGridPens ENDP

DrawScopeGrid PROC STDCALL hdc:DWORD, pRect:DWORD
    LOCAL gridX:DWORD
    LOCAL gridY:DWORD
    LOCAL leftEdge:DWORD
    LOCAL topEdge:DWORD
    LOCAL rightEdge:DWORD
    LOCAL bottomEdge:DWORD
    LOCAL centerX:DWORD
    LOCAL centerY:DWORD
    LOCAL oldPen:DWORD

    call EnsureGridPens
    cmp gGridMinorPen, 0
    je ScopeGridDone

    mov edx, pRect
    mov eax, DWORD PTR [edx]
    mov leftEdge, eax
    mov eax, DWORD PTR [edx+4]
    mov topEdge, eax
    mov eax, DWORD PTR [edx+8]
    mov rightEdge, eax
    mov eax, DWORD PTR [edx+12]
    mov bottomEdge, eax

    invoke SelectObject, hdc, gGridMinorPen
    mov oldPen, eax

    mov eax, leftEdge
    mov gridX, eax
MinorVerticalLoop:
    mov eax, gridX
    cmp eax, rightEdge
    jge MinorHorizontalSetup
    invoke MoveToEx, hdc, gridX, topEdge, NULL
    invoke LineTo, hdc, gridX, bottomEdge
    add gridX, GRID_MINOR_STEP
    jmp MinorVerticalLoop

MinorHorizontalSetup:
    mov eax, topEdge
    mov gridY, eax
MinorHorizontalLoop:
    mov eax, gridY
    cmp eax, bottomEdge
    jge MajorGridSetup
    invoke MoveToEx, hdc, leftEdge, gridY, NULL
    invoke LineTo, hdc, rightEdge, gridY
    add gridY, GRID_MINOR_STEP
    jmp MinorHorizontalLoop

MajorGridSetup:
    cmp gGridMajorPen, 0
    je CenterAxisSetup
    invoke SelectObject, hdc, gGridMajorPen

    mov eax, leftEdge
    mov gridX, eax
MajorVerticalLoop:
    mov eax, gridX
    cmp eax, rightEdge
    jge MajorHorizontalSetup
    invoke MoveToEx, hdc, gridX, topEdge, NULL
    invoke LineTo, hdc, gridX, bottomEdge
    add gridX, GRID_MAJOR_STEP
    jmp MajorVerticalLoop

MajorHorizontalSetup:
    mov eax, topEdge
    mov gridY, eax
MajorHorizontalLoop:
    mov eax, gridY
    cmp eax, bottomEdge
    jge CenterAxisSetup
    invoke MoveToEx, hdc, leftEdge, gridY, NULL
    invoke LineTo, hdc, rightEdge, gridY
    add gridY, GRID_MAJOR_STEP
    jmp MajorHorizontalLoop

CenterAxisSetup:
    cmp gGridAxisPen, 0
    je ScopeGridRestore
    invoke SelectObject, hdc, gGridAxisPen

    mov eax, leftEdge
    add eax, rightEdge
    sar eax, 1
    mov centerX, eax
    mov eax, topEdge
    add eax, bottomEdge
    sar eax, 1
    mov centerY, eax

    invoke MoveToEx, hdc, centerX, topEdge, NULL
    invoke LineTo, hdc, centerX, bottomEdge
    invoke MoveToEx, hdc, leftEdge, centerY, NULL
    invoke LineTo, hdc, rightEdge, centerY

ScopeGridRestore:
    invoke SelectObject, hdc, oldPen
ScopeGridDone:
    ret
DrawScopeGrid ENDP

EnsureSupersampleSurface PROC STDCALL referenceDC:DWORD, wantedWidth:DWORD, wantedHeight:DWORD
    cmp gScopeDC, 0
    je CreateScopeSurface

    mov eax, gScopeWidth
    cmp eax, wantedWidth
    jne RecreateScopeSurface
    mov eax, gScopeHeight
    cmp eax, wantedHeight
    jne RecreateScopeSurface

    mov eax, gScopeDC
    ret

RecreateScopeSurface:
    call DestroyVisualizerResources

CreateScopeSurface:
    invoke CreateCompatibleDC, referenceDC
    test eax, eax
    jz ScopeSurfaceFailed
    mov gScopeDC, eax

    invoke CreateCompatibleBitmap, referenceDC, wantedWidth, wantedHeight
    test eax, eax
    jz ScopeBitmapFailed
    mov gScopeBitmap, eax

    invoke SelectObject, gScopeDC, gScopeBitmap
    mov gScopeOldBitmap, eax

    mov eax, wantedWidth
    mov gScopeWidth, eax
    mov eax, wantedHeight
    mov gScopeHeight, eax

    mov eax, gScopeDC
    ret

ScopeBitmapFailed:
    invoke DeleteDC, gScopeDC
    mov gScopeDC, 0
ScopeSurfaceFailed:
    xor eax, eax
    ret
EnsureSupersampleSurface ENDP

SinPhase16 PROC STDCALL phaseValue:DWORD
    LOCAL fracValue:DWORD
    LOCAL value0:DWORD
    LOCAL value1:DWORD

    mov eax, phaseValue
    and eax, PHASE_MASK

    mov edx, eax
    and edx, 0FFh
    mov fracValue, edx

    shr eax, 8
    and eax, 0FFh
    mov ecx, sineTable[eax*4]
    mov value0, ecx

    inc eax
    and eax, 0FFh
    mov ecx, sineTable[eax*4]
    mov value1, ecx

    mov eax, value1
    sub eax, value0
    mov ecx, fracValue
    imul eax, ecx
    sar eax, 8
    add eax, value0
    ret
SinPhase16 ENDP

ProjectVectorPoint PROC STDCALL pRect:DWORD, logicalX:DWORD, logicalY:DWORD, depthValue:DWORD, sinYValue:DWORD, cosYValue:DWORD, sinXValue:DWORD, cosXValue:DWORD, sinZValue:DWORD, cosZValue:DWORD, pOutX:DWORD, pOutY:DWORD
    LOCAL modelX:DWORD
    LOCAL modelY:DWORD
    LOCAL rotX1:DWORD
    LOCAL rotY2:DWORD
    LOCAL rotZ1:DWORD
    LOCAL rotZ2:DWORD
    LOCAL finalX:DWORD
    LOCAL finalY:DWORD
    LOCAL denomValue:DWORD
    LOCAL centerX:DWORD
    LOCAL centerY:DWORD

    mov eax, logicalX
    sub eax, MODEL_CENTER_X
    imul eax, MODEL_SCALE
    mov modelX, eax

    mov eax, MODEL_CENTER_Y
    sub eax, logicalY
    imul eax, MODEL_SCALE
    mov modelY, eax

    ; Y rotation.
    mov eax, modelX
    imul eax, cosYValue
    mov ecx, depthValue
    imul ecx, sinYValue
    add eax, ecx
    sar eax, 10
    mov rotX1, eax

    mov eax, modelX
    imul eax, sinYValue
    neg eax
    mov ecx, depthValue
    imul ecx, cosYValue
    add eax, ecx
    sar eax, 10
    mov rotZ1, eax

    ; X rotation.
    mov eax, modelY
    imul eax, cosXValue
    mov ecx, rotZ1
    imul ecx, sinXValue
    sub eax, ecx
    sar eax, 10
    mov rotY2, eax

    mov eax, modelY
    imul eax, sinXValue
    mov ecx, rotZ1
    imul ecx, cosXValue
    add eax, ecx
    sar eax, 10
    mov rotZ2, eax

    ; Z rotation.
    mov eax, rotX1
    imul eax, cosZValue
    mov ecx, rotY2
    imul ecx, sinZValue
    sub eax, ecx
    sar eax, 10
    mov finalX, eax

    mov eax, rotX1
    imul eax, sinZValue
    mov ecx, rotY2
    imul ecx, cosZValue
    add eax, ecx
    sar eax, 10
    mov finalY, eax

    ; Viewport center.
    mov edx, pRect
    mov eax, DWORD PTR [edx]
    add eax, DWORD PTR [edx+8]
    sar eax, 1
    mov centerX, eax

    mov eax, DWORD PTR [edx+4]
    add eax, DWORD PTR [edx+12]
    sar eax, 1
    mov centerY, eax

    mov ecx, FOCAL_LENGTH
    add ecx, rotZ2
    cmp ecx, MIN_PERSPECTIVE_DENOM
    jge DenomReady
    mov ecx, MIN_PERSPECTIVE_DENOM
DenomReady:
    mov denomValue, ecx

    mov eax, finalX
    imul eax, FOCAL_LENGTH
    cdq
    idiv denomValue
    add eax, centerX
    mov edx, pOutX
    mov DWORD PTR [edx], eax

    mov eax, finalY
    imul eax, FOCAL_LENGTH
    cdq
    idiv denomValue
    mov ecx, centerY
    sub ecx, eax
    mov edx, pOutY
    mov DWORD PTR [edx], ecx

    ret
ProjectVectorPoint ENDP

DrawSegment3D PROC STDCALL hdc:DWORD, pRect:DWORD, pSegment:DWORD, sinYValue:DWORD, cosYValue:DWORD, sinXValue:DWORD, cosXValue:DWORD, sinZValue:DWORD, cosZValue:DWORD, farPen:DWORD, sidePen:DWORD, nearPen:DWORD, beamPen:DWORD, highlightFlag:DWORD
    LOCAL axNear:DWORD
    LOCAL ayNear:DWORD
    LOCAL bxNear:DWORD
    LOCAL byNear:DWORD
    LOCAL axFar:DWORD
    LOCAL ayFar:DWORD
    LOCAL bxFar:DWORD
    LOCAL byFar:DWORD
    LOCAL nearDepth:DWORD
    LOCAL farDepth:DWORD
    LOCAL oldPen:DWORD
    LOCAL xA:DWORD
    LOCAL yA:DWORD
    LOCAL xB:DWORD
    LOCAL yB:DWORD

    push esi

    mov eax, cosYValue
    test eax, eax
    js BackPlaneIsNear
    mov nearDepth, -MODEL_DEPTH
    mov farDepth, MODEL_DEPTH
    jmp DepthReady
BackPlaneIsNear:
    mov nearDepth, MODEL_DEPTH
    mov farDepth, -MODEL_DEPTH
DepthReady:

    mov esi, pSegment
    mov eax, DWORD PTR [esi]
    mov xA, eax
    mov eax, DWORD PTR [esi+4]
    mov yA, eax
    mov eax, DWORD PTR [esi+8]
    mov xB, eax
    mov eax, DWORD PTR [esi+12]
    mov yB, eax

    invoke ProjectVectorPoint, pRect, xA, yA, nearDepth, sinYValue, cosYValue, sinXValue, cosXValue, sinZValue, cosZValue, ADDR axNear, ADDR ayNear
    invoke ProjectVectorPoint, pRect, xB, yB, nearDepth, sinYValue, cosYValue, sinXValue, cosXValue, sinZValue, cosZValue, ADDR bxNear, ADDR byNear
    invoke ProjectVectorPoint, pRect, xA, yA, farDepth, sinYValue, cosYValue, sinXValue, cosXValue, sinZValue, cosZValue, ADDR axFar, ADDR ayFar
    invoke ProjectVectorPoint, pRect, xB, yB, farDepth, sinYValue, cosYValue, sinXValue, cosXValue, sinZValue, cosZValue, ADDR bxFar, ADDR byFar

    invoke SelectObject, hdc, farPen
    mov oldPen, eax
    invoke MoveToEx, hdc, axFar, ayFar, NULL
    invoke LineTo, hdc, bxFar, byFar

    invoke SelectObject, hdc, sidePen
    invoke MoveToEx, hdc, axFar, ayFar, NULL
    invoke LineTo, hdc, axNear, ayNear
    invoke MoveToEx, hdc, bxFar, byFar, NULL
    invoke LineTo, hdc, bxNear, byNear

    invoke SelectObject, hdc, nearPen
    invoke MoveToEx, hdc, axNear, ayNear, NULL
    invoke LineTo, hdc, bxNear, byNear

    cmp highlightFlag, 0
    je SegmentRestorePen
    invoke SelectObject, hdc, beamPen
    invoke MoveToEx, hdc, axNear, ayNear, NULL
    invoke LineTo, hdc, bxNear, byNear
    mov eax, bxNear
    add eax, 4
    mov ecx, byNear
    add ecx, 2
    invoke LineTo, hdc, eax, ecx

SegmentRestorePen:
    invoke SelectObject, hdc, oldPen
    pop esi
    ret
DrawSegment3D ENDP

DrawModelPass PROC STDCALL hdc:DWORD, pRect:DWORD, phaseValue:DWORD, farColor:DWORD, sideColor:DWORD, nearColor:DWORD, beamColor:DWORD, penWidth:DWORD, beamIndex:DWORD
    LOCAL basePhase:DWORD
    LOCAL yawPhase:DWORD
    LOCAL pitchPhase:DWORD
    LOCAL rollPhase:DWORD
    LOCAL harmonicA:DWORD
    LOCAL harmonicB:DWORD
    LOCAL sinYValue:DWORD
    LOCAL cosYValue:DWORD
    LOCAL sinXValue:DWORD
    LOCAL cosXValue:DWORD
    LOCAL sinZValue:DWORD
    LOCAL cosZValue:DWORD
    LOCAL farPen:DWORD
    LOCAL sidePen:DWORD
    LOCAL nearPen:DWORD
    LOCAL beamPen:DWORD
    LOCAL actualPenWidth:DWORD
    LOCAL segIndex:DWORD
    LOCAL pCurrent:DWORD
    LOCAL isHighlight:DWORD

    mov eax, phaseValue
    and eax, PHASE_MASK
    mov basePhase, eax

    mov eax, basePhase
    shl eax, 1
    add eax, 7424
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    imul eax, 1500
    sar eax, 10
    mov harmonicA, eax

    mov eax, basePhase
    imul eax, 3
    add eax, 21845
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    imul eax, 420
    sar eax, 10
    mov harmonicB, eax

    mov eax, basePhase
    add eax, harmonicA
    add eax, harmonicB
    and eax, PHASE_MASK
    mov yawPhase, eax

    mov eax, basePhase
    shl eax, 1
    add eax, 4352
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    imul eax, 4600
    sar eax, 10
    mov harmonicA, eax

    mov eax, basePhase
    imul eax, 5
    add eax, 12800
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    imul eax, 620
    sar eax, 10
    add eax, harmonicA
    and eax, PHASE_MASK
    mov pitchPhase, eax

    mov eax, basePhase
    imul eax, 3
    add eax, 18688
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    imul eax, 2450
    sar eax, 10
    mov harmonicA, eax

    mov eax, basePhase
    add eax, 32768
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    imul eax, 520
    sar eax, 10
    add eax, harmonicA
    and eax, PHASE_MASK
    mov rollPhase, eax

    invoke SinPhase16, yawPhase
    mov sinYValue, eax
    mov eax, yawPhase
    add eax, PHASE_QUARTER
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    mov cosYValue, eax

    invoke SinPhase16, pitchPhase
    mov sinXValue, eax
    mov eax, pitchPhase
    add eax, PHASE_QUARTER
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    mov cosXValue, eax

    invoke SinPhase16, rollPhase
    mov sinZValue, eax
    mov eax, rollPhase
    add eax, PHASE_QUARTER
    and eax, PHASE_MASK
    invoke SinPhase16, eax
    mov cosZValue, eax

    mov eax, penWidth
    shl eax, 1
    cmp eax, 1
    jge PenWidthReady
    mov eax, 1
PenWidthReady:
    mov actualPenWidth, eax

    invoke CreatePen, PS_SOLID, actualPenWidth, farColor
    mov farPen, eax
    invoke CreatePen, PS_SOLID, actualPenWidth, sideColor
    mov sidePen, eax
    invoke CreatePen, PS_SOLID, actualPenWidth, nearColor
    mov nearPen, eax
    invoke CreatePen, PS_SOLID, 6, beamColor
    mov beamPen, eax

    mov segIndex, 0
    mov pCurrent, OFFSET vectorSegments

ModelSegmentLoop:
    mov eax, segIndex
    cmp eax, VECTOR_SEGMENT_COUNT
    jae ModelSegmentsDone

    mov isHighlight, 0
    mov ecx, beamIndex
    cmp ecx, 0FFFFFFFFh
    je NoHighlightForPass
    cmp eax, ecx
    jne CheckPreviousBeamStroke
    mov isHighlight, 1
    jmp NoHighlightForPass
CheckPreviousBeamStroke:
    mov edx, ecx
    dec edx
    jns PreviousIndexReady
    mov edx, VECTOR_SEGMENT_COUNT - 1
PreviousIndexReady:
    cmp eax, edx
    jne NoHighlightForPass
    mov isHighlight, 1

NoHighlightForPass:
    invoke DrawSegment3D, hdc, pRect, pCurrent, sinYValue, cosYValue, sinXValue, cosXValue, sinZValue, cosZValue, farPen, sidePen, nearPen, beamPen, isHighlight

    add pCurrent, VEC_ENTRY_BYTES
    inc segIndex
    jmp ModelSegmentLoop

ModelSegmentsDone:
    invoke DeleteObject, farPen
    invoke DeleteObject, sidePen
    invoke DeleteObject, nearPen
    invoke DeleteObject, beamPen
    ret
DrawModelPass ENDP

DrawVisualizer PROC STDCALL hdc:DWORD, pRect:DWORD
    LOCAL phaseNow:DWORD
    LOCAL phaseGhost1:DWORD
    LOCAL phaseGhost2:DWORD
    LOCAL phaseGhost3:DWORD
    LOCAL phaseGhost4:DWORD
    LOCAL beamIndex:DWORD
    LOCAL elapsedMs:DWORD
    LOCAL rectWidth:DWORD
    LOCAL rectHeight:DWORD
    LOCAL hiWidth:DWORD
    LOCAL hiHeight:DWORD
    LOCAL hiRect:RECT
    LOCAL ssDC:DWORD
    LOCAL bgBrush:DWORD

    invoke timeGetTime
    sub eax, gVisualStartTick
    mov elapsedMs, eax

    xor edx, edx
    mov ecx, ROTATION_PERIOD_MS
    div ecx
    ; EDX = elapsed modulo one base revolution.
    mov eax, edx
    shl eax, 16
    xor edx, edx
    mov ecx, ROTATION_PERIOD_MS
    div ecx
    and eax, PHASE_MASK
    mov phaseNow, eax
    mov gVisualPhase, eax

    mov ecx, eax
    sub ecx, 90
    and ecx, PHASE_MASK
    mov phaseGhost1, ecx

    mov ecx, eax
    sub ecx, 180
    and ecx, PHASE_MASK
    mov phaseGhost2, ecx

    mov ecx, eax
    sub ecx, 360
    and ecx, PHASE_MASK
    mov phaseGhost3, ecx

    mov ecx, eax
    sub ecx, 720
    and ecx, PHASE_MASK
    mov phaseGhost4, ecx

    ; Four beam sweeps per base revolution.
    mov eax, phaseNow
    shr eax, 8
    and eax, 63
    mov ecx, VECTOR_SEGMENT_COUNT
    imul eax, ecx
    shr eax, 6
    cmp eax, VECTOR_SEGMENT_COUNT
    jb BeamReady
    mov eax, VECTOR_SEGMENT_COUNT - 1
BeamReady:
    mov beamIndex, eax

    ; Destination size.
    mov edx, pRect
    mov eax, DWORD PTR [edx+8]
    sub eax, DWORD PTR [edx]
    mov rectWidth, eax
    shl eax, 1
    mov hiWidth, eax

    mov eax, DWORD PTR [edx+12]
    sub eax, DWORD PTR [edx+4]
    mov rectHeight, eax
    shl eax, 1
    mov hiHeight, eax

    cmp hiWidth, 0
    jle VisualizerDone
    cmp hiHeight, 0
    jle VisualizerDone

    invoke EnsureSupersampleSurface, hdc, hiWidth, hiHeight
    test eax, eax
    jz VisualizerDone
    mov ssDC, eax

    mov hiRect.left, 0
    mov hiRect.top, 0
    mov eax, hiWidth
    mov hiRect.right, eax
    mov eax, hiHeight
    mov hiRect.bottom, eax

    invoke CreateSolidBrush, 00000000h
    mov bgBrush, eax
    invoke FillRect, ssDC, ADDR hiRect, bgBrush
    invoke DeleteObject, bgBrush

    invoke DrawScopeGrid, ssDC, ADDR hiRect

    invoke DrawModelPass, ssDC, ADDR hiRect, phaseGhost4, SCOPE_GREEN_GHOST_2, SCOPE_GREEN_GHOST_2, SCOPE_GREEN_GHOST_2, SCOPE_GREEN_GHOST_2, 1, 0FFFFFFFFh
    invoke DrawModelPass, ssDC, ADDR hiRect, phaseGhost3, SCOPE_GREEN_GHOST_1, SCOPE_GREEN_GHOST_1, SCOPE_GREEN_GHOST_1, SCOPE_GREEN_GHOST_1, 1, 0FFFFFFFFh
    invoke DrawModelPass, ssDC, ADDR hiRect, phaseGhost2, SCOPE_GREEN_FAR, SCOPE_GREEN_GHOST_1, SCOPE_GREEN_GHOST_1, SCOPE_GREEN_GHOST_1, 1, 0FFFFFFFFh
    invoke DrawModelPass, ssDC, ADDR hiRect, phaseGhost1, SCOPE_GREEN_SIDE, SCOPE_GREEN_SIDE, SCOPE_GREEN_SIDE, SCOPE_GREEN_SIDE, 1, 0FFFFFFFFh

    invoke DrawModelPass, ssDC, ADDR hiRect, phaseNow, SCOPE_GREEN_GHOST_1, SCOPE_GREEN_SIDE, SCOPE_GREEN_SIDE, SCOPE_GREEN_SIDE, 3, 0FFFFFFFFh
    invoke DrawModelPass, ssDC, ADDR hiRect, phaseNow, SCOPE_GREEN_FAR, SCOPE_GREEN_SIDE, SCOPE_GREEN_NEAR, SCOPE_GREEN_BEAM, 1, beamIndex

    invoke SetStretchBltMode, hdc, HALFTONE

    mov edx, pRect
    mov eax, DWORD PTR [edx]
    mov ecx, DWORD PTR [edx+4]
    invoke StretchBlt, hdc, eax, ecx, rectWidth, rectHeight, ssDC, 0, 0, hiWidth, hiHeight, SRCCOPY

VisualizerDone:
    ret
DrawVisualizer ENDP

END
