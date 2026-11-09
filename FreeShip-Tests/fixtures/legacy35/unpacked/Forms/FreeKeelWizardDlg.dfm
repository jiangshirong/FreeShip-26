object FreeKeelWizardDialog: TFreeKeelWizardDialog
  Left = 240
  Top = 116
  BorderStyle = bsDialog
  Caption = 'Keel and rudder wizard'
  ClientHeight = 488
  ClientWidth = 720
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = True
  Position = poMainFormCenter
  PixelsPerInch = 96
  TextHeight = 13
  object Panel2: TPanel
    Left = 0
    Top = 22
    Width = 720
    Height = 453
    Align = alClient
    BevelOuter = bvLowered
    BorderWidth = 10
    TabOrder = 0
    object GroupBox1: TGroupBox
      Left = 8
      Top = 276
      Width = 236
      Height = 167
      Caption = 'Properties'
      TabOrder = 0
      object Label7: TLabel
        Left = 7
        Top = 68
        Width = 65
        Height = 13
        Caption = 'Planform area'
      end
      object _Label8: TLabel
        Left = 150
        Top = 68
        Width = 38
        Height = 13
        Caption = '_Label8'
      end
      object Label9: TLabel
        Left = 7
        Top = 84
        Width = 67
        Height = 13
        Caption = 'Planform COG'
      end
      object _Label10: TLabel
        Left = 150
        Top = 84
        Width = 44
        Height = 13
        Caption = '_Label10'
      end
      object Label11: TLabel
        Left = 7
        Top = 100
        Width = 89
        Height = 13
        Caption = 'Mean chord length'
      end
      object _Label12: TLabel
        Left = 150
        Top = 100
        Width = 38
        Height = 13
        Caption = 'Label10'
      end
      object Label13: TLabel
        Left = 7
        Top = 132
        Width = 106
        Height = 13
        Caption = 'Geometric aspect ratio'
      end
      object _Label14: TLabel
        Left = 150
        Top = 132
        Width = 38
        Height = 13
        Caption = 'Label10'
      end
      object Label15: TLabel
        Left = 7
        Top = 148
        Width = 100
        Height = 13
        Caption = 'Effective aspect ratio'
      end
      object _Label16: TLabel
        Left = 150
        Top = 148
        Width = 38
        Height = 13
        Caption = 'Label10'
      end
      object Label18: TLabel
        Left = 7
        Top = 20
        Width = 35
        Height = 13
        Caption = 'Volume'
      end
      object _Label19: TLabel
        Left = 150
        Top = 20
        Width = 32
        Height = 13
        Caption = 'Label8'
      end
      object Label20: TLabel
        Left = 7
        Top = 36
        Width = 61
        Height = 13
        Caption = 'Volume COG'
      end
      object _Label21: TLabel
        Left = 150
        Top = 36
        Width = 32
        Height = 13
        Caption = 'Label8'
      end
      object Label22: TLabel
        Left = 7
        Top = 52
        Width = 59
        Height = 13
        Caption = 'Wetted area'
      end
      object _Label23: TLabel
        Left = 150
        Top = 52
        Width = 32
        Height = 13
        Caption = 'Label8'
      end
      object Label24: TLabel
        Left = 7
        Top = 116
        Width = 49
        Height = 13
        Caption = 'Thickness'
      end
      object _Label25: TLabel
        Left = 150
        Top = 116
        Width = 38
        Height = 13
        Caption = 'Label10'
      end
    end
    object PageControl: TPageControl
      Left = 254
      Top = 11
      Width = 455
      Height = 431
      ActivePage = TabSheet1
      Align = alRight
      TabOrder = 1
      object TabSheet1: TTabSheet
        Caption = '3D view'
        object Viewport: TFreeViewport
          Left = 0
          Top = 0
          Width = 447
          Height = 403
          Angle = 20.000000000000000000
          Align = alClient
          BackgroundImage.Alpha = 255
          BackgroundImage.Tolerance = 5
          BackgroundImage.Transparent = False
          BackgroundImage.TransparentColor = clBlack
          BackgroundImage.Visible = True
          BevelOuter = bvNone
          BorderStyle = bsSingle
          CameraType = ftFarTele
          Color = clWhite
          DoubleBuffer = True
          Elevation = 20.000000000000000000
          ViewType = fvPerspective
          ViewportMode = vmWireFrame
          OnMouseDown = ViewportMouseDown
          OnMouseMove = ViewportMouseMove
          OnRedraw = ViewportRedraw
          OnRequestExtents = ViewportRequestExtents
        end
      end
      object TabSheet2: TTabSheet
        Caption = 'Lift and drag'
        ImageIndex = 1
        object Chart: TChart
          Left = 0
          Top = 0
          Width = 447
          Height = 403
          AnimatedZoom = True
          BackWall.Brush.Color = clWhite
          BackWall.Brush.Style = bsClear
          MarginRight = 4
          Title.AdjustFrame = False
          Title.Color = clBtnFace
          Title.Text.Strings = (
            'Chart')
          Title.Visible = False
          BottomAxis.MinorTickCount = 4
          BottomAxis.MinorTickLength = 3
          BottomAxis.Title.Caption = 'Angle of attack [degr]'
          Chart3DPercent = 20
          LeftAxis.AxisValuesFormat = '#0.00'
          LeftAxis.LabelStyle = talValue
          LeftAxis.MinorTickCount = 1
          LeftAxis.MinorTickLength = 3
          LeftAxis.Title.Caption = 'Cx, Cy, Cd, m_z, 0.1*K'
          LeftAxis.TitleSize = 4
          Legend.Alignment = laTop
          Legend.ColorWidth = 50
          Legend.TextStyle = ltsPlain
          Legend.TopPos = 45
          RightAxis.AxisValuesFormat = '#,##0.#'
          RightAxis.Grid.Visible = False
          RightAxis.MinorTickCount = 1
          RightAxis.Title.Angle = 90
          RightAxis.Title.Caption = 'Cy/Cx'
          RightAxis.TitleSize = 4
          View3D = False
          View3DOptions.Elevation = 360
          View3DOptions.HorizOffset = 16
          View3DOptions.Orthogonal = False
          View3DOptions.Rotation = 338
          View3DOptions.Zoom = 103
          Align = alClient
          BevelOuter = bvLowered
          TabOrder = 0
          object Series1: TLineSeries
            Marks.ArrowLength = 8
            Marks.Style = smsValue
            Marks.Visible = False
            SeriesColor = 11162880
            Title = 'Cy'
            LinePen.Width = 2
            Pointer.HorizSize = 2
            Pointer.InflateMargins = False
            Pointer.Style = psRectangle
            Pointer.VertSize = 2
            Pointer.Visible = False
            XValues.DateTime = False
            XValues.Name = 'X'
            XValues.Multiplier = 1.000000000000000000
            XValues.Order = loAscending
            YValues.DateTime = False
            YValues.Name = 'Y'
            YValues.Multiplier = 1.000000000000000000
            YValues.Order = loNone
          end
          object Series2: TLineSeries
            Marks.ArrowLength = 8
            Marks.Visible = False
            SeriesColor = clGreen
            Title = 'Cx'
            LinePen.Width = 2
            Pointer.InflateMargins = True
            Pointer.Style = psRectangle
            Pointer.Visible = False
            XValues.DateTime = False
            XValues.Name = 'X'
            XValues.Multiplier = 1.000000000000000000
            XValues.Order = loAscending
            YValues.DateTime = False
            YValues.Name = 'Y'
            YValues.Multiplier = 1.000000000000000000
            YValues.Order = loNone
          end
          object Series3: TLineSeries
            Marks.ArrowLength = 8
            Marks.Visible = False
            SeriesColor = clRed
            Title = 'm_z'
            LinePen.Width = 2
            Pointer.InflateMargins = True
            Pointer.Style = psRectangle
            Pointer.Visible = False
            XValues.DateTime = False
            XValues.Name = 'X'
            XValues.Multiplier = 1.000000000000000000
            XValues.Order = loAscending
            YValues.DateTime = False
            YValues.Name = 'Y'
            YValues.Multiplier = 1.000000000000000000
            YValues.Order = loNone
          end
          object Series4: TLineSeries
            Marks.ArrowLength = 8
            Marks.Visible = False
            SeriesColor = clBlue
            Title = 'Cd'
            LinePen.Width = 2
            Pointer.InflateMargins = True
            Pointer.Style = psRectangle
            Pointer.Visible = False
            XValues.DateTime = False
            XValues.Name = 'X'
            XValues.Multiplier = 1.000000000000000000
            XValues.Order = loAscending
            YValues.DateTime = False
            YValues.Name = 'Y'
            YValues.Multiplier = 1.000000000000000000
            YValues.Order = loNone
          end
          object Series5: TLineSeries
            Marks.ArrowLength = 8
            Marks.Visible = False
            SeriesColor = clRed
            Title = 'K=Cy/Cx'
            LinePen.Width = 2
            Pointer.InflateMargins = True
            Pointer.Style = psRectangle
            Pointer.Visible = False
            XValues.DateTime = False
            XValues.Name = 'X'
            XValues.Multiplier = 1.000000000000000000
            XValues.Order = loAscending
            YValues.DateTime = False
            YValues.Name = 'Y'
            YValues.Multiplier = 1.000000000000000000
            YValues.Order = loNone
          end		  
        end
      end
    end
    object PageControl1: TPageControl
      Left = 8
      Top = 8
      Width = 236
      Height = 267
      ActivePage = TabSheet3
      TabOrder = 2
      object TabSheet3: TTabSheet
        Caption = 'Keel'
        object Label1: TLabel
          Left = 0
          Top = 9
          Width = 93
          Height = 13
          Caption = 'Type of appendage'
        end
        object Label2: TLabel
          Left = 0
          Top = 33
          Width = 73
          Height = 13
          Caption = 'Planform shape'
        end
        object Label17: TLabel
          Left = 0
          Top = 57
          Width = 62
          Height = 13
          Caption = 'Wing section'
        end
        object Label3: TLabel
          Left = 0
          Top = 81
          Width = 85
          Height = 13
          Caption = 'Root chord length'
        end
        object Label4: TLabel
          Left = 0
          Top = 105
          Width = 77
          Height = 13
          Caption = 'Tip chord length'
        end
        object Label6: TLabel
          Left = 0
          Top = 129
          Width = 39
          Height = 13
          Caption = 'Delta tip'
        end
        object Label5: TLabel
          Left = 0
          Top = 153
          Width = 25
          Height = 13
          Caption = 'Span'
        end
        object Label26: TLabel
          Left = 0
          Top = 177
          Width = 85
          Height = 13
          Caption = 'No. vertical points'
        end
        object Label27: TLabel
          Left = 0
          Top = 201
          Width = 93
          Height = 13
          Caption = 'No horizontal points'
        end
        object Label28: TLabel
          Left = 0
          Top = 221
          Width = 97
          Height = 13
          Caption = 'Vertical compression'
        end
        object ComboBox: TComboBox
          Left = 102
          Top = 1
          Width = 125
          Height = 21
          ItemHeight = 13
          ItemIndex = 0
          TabOrder = 0
          Text = 'Keel'
          OnClick = ComboBoxClick
          Items.Strings = (
            'Keel'
            'Rudder')
        end
        object ComboBox1: TComboBox
          Left = 102
          Top = 25
          Width = 125
          Height = 21
          ItemHeight = 13
          ItemIndex = 0
          TabOrder = 1
          Text = 'Trapezoidal'
          OnClick = ComboBox1Click
          Items.Strings = (
            'Trapezoidal'
            'Semi ellipse'
            'Quarter ellipse'
            'Straight quarter chord line')
        end
        object _ComboBox2: TComboBox
          Left = 102
          Top = 49
          Width = 125
          Height = 21
          ItemHeight = 13
          ItemIndex = 0
          TabOrder = 2
          Text = 'NACA63-006'
          OnClick = ComboBox1Click
          Items.Strings = (
        'NACA63-006'
        'NACA63-009'
        'NACA63-010'
        'NACA63-012'
        'NACA63-015'
        'NACA63-018'
        'NACA63-021'
        'NACA63-010a'
        'NACA63-012a'
        'NACA63-015a'
        'NACA64-006'
        'NACA64-008'
        'NACA64-009'
        'NACA64-010'
        'NACA64-012'
        'NACA64-015'
        'NACA64-018'
        'NACA64-021'
        'NACA65-006'
        'NACA65-008'
        'NACA65-009'
        'NACA65-010'
        'NACA65-012'
        'NACA65-015'
        'NACA65-018'
        'NACA65-021'
        'NACA66-009'
        'NACA66-010'
        'NACA66-018'
        'NACA66-021'
        'NACA0006'
        'NACA0008'
        'NACA0009'
        'NACA0010'
        'NACA0012'
        'NACA0015'
        'NACA0018'
        'NACA0020'
        'NACA0021'
        'NACA0025'
        'NACA0030'
        'NACA0035'
        'Jouk-06'
        'Jouk-08'
        'Jouk-09'
        'Jouk-10'
        'Jouk-12'
        'Jouk-15'
        'Jouk-18'
        'Jouk-20'
        'Jouk-21'
        'Jouk-25'
        'Jouk-30'
        'Jouk-35'
        'E168'
        'E169'
        'E171(Dicke 12.28%)'
        'E297'
        'E472'
        'E473'
        'E474'
        'E475'
        'E476'
        'E477'
        'E478'
        'E479'
        'E520'
        'E521'
        'E836'
        'E837'
        'E838'
        'E862 STRUT'
        'E864 STRUT'
        'EA 6(-1)-009'
        'EA 6(-1)-012'
        'EA 6(-1)-006'
        'GOE 409'
        'GOE 410'
        'GOE 411'
        'GOE 443'
        'GOE 444'
        'GOE 445'
        'GOE 459'
        'GOE 460'
        'GOE 775'
        'GOE 776'
        'RAF 27'
        'RAF 30'
        'RAF 30 MOD'
        'S1010 HPV'
        'S1012'
        'S1014'
        'S1016'
        'S1046 17%'
        'S1048 14%'
        'S8035 14%'
        'SD8020-010-88'
        'NACA-M1'
        'NACA-M2'
        'NACA-M3'
        'NACA-0009sm'
        'NACA-0012'
        'NACA64-008a'
        'NACA64-010a'
        'NACA64-012a'
        'NACA642-015'
        'NACA642-015a'
        'NACA/LANGLEY LS(1)'
        'NACA/LANGLEY 64-012'
        'NACA/LANGLEY SC'
        'HQ 0/7'
        'HQ 0/9'
        'HQ 0/10'
        'WORTMANN FX 71-089A'
        'FX 71-120'
        'FX 71-L-150/20'
        'FX 71-L-150/25'
        'FX 71-L-150/K30'
        'FX LIII-142 K25'
        'FX L V-152'
        'BELL 540'
        'RYAN BQM-34'
        'EH,0.0/9.0'
        'Griffith 30%'
        'JOUKOWSKI 12%'
)
        end
        object Input1: TFreeNumInput
          Left = 162
          Top = 73
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 2.000000000000000000
          Validate = False
          OnAfterSetValue = Input1AfterSetValue
          TabOrder = 3
        end
        object Input2: TFreeNumInput
          Left = 162
          Top = 97
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 1.000000000000000000
          Validate = False
          OnAfterSetValue = Input1AfterSetValue
          TabOrder = 4
        end
        object Input4: TFreeNumInput
          Left = 162
          Top = 121
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 0.100000000000000000
          Validate = False
          OnAfterSetValue = Input1AfterSetValue
          TabOrder = 5
        end
        object Input3: TFreeNumInput
          Left = 162
          Top = 145
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 1.000000000000000000
          Validate = False
          OnAfterSetValue = Input1AfterSetValue
          TabOrder = 6
        end
        object Input5: TFreeNumInput
          Left = 162
          Top = 169
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 0
          Digits = 12
          DataType = dtInteger
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 6.000000000000000000
          Validate = False
          OnAfterSetValue = Input5AfterSetValue
          TabOrder = 7
        end
        object Input6: TFreeNumInput
          Left = 162
          Top = 193
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 0
          Digits = 12
          DataType = dtInteger
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 15.000000000000000000
          Validate = False
          OnAfterSetValue = Input6AfterSetValue
          TabOrder = 8
        end
        object TrackBar1: TTrackBar
          Left = 116
          Top = 215
          Width = 115
          Height = 23
          Max = 20
          TabOrder = 9
          ThumbLength = 15
          OnChange = TrackBar1Change
        end
      end
      object TabSheet4: TTabSheet
        Caption = 'Bulb'
        ImageIndex = 1
        object Label8: TLabel
          Left = 0
          Top = 9
          Width = 73
          Height = 13
          Caption = 'Planform shape'
        end
        object Label12: TLabel
          Left = 0
          Top = 33
          Width = 62
          Height = 13
          Caption = 'Wing section'
        end
        object Label14: TLabel
          Left = 0
          Top = 57
          Width = 33
          Height = 13
          Caption = 'Length'
        end
        object Label16: TLabel
          Left = 0
          Top = 201
          Width = 93
          Height = 13
          Caption = 'No horizontal points'
        end
        object Label21: TLabel
          Left = 0
          Top = 153
          Width = 72
          Height = 13
          Caption = 'Specific weight'
          Enabled = False
        end
        object Label23: TLabel
          Left = 0
          Top = 221
          Width = 97
          Height = 13
          Caption = 'Vertical compression'
        end
        object Label25: TLabel
          Left = 0
          Top = 81
          Width = 39
          Height = 13
          Caption = 'Delta tip'
        end
        object Label29: TLabel
          Left = 0
          Top = 177
          Width = 55
          Height = 13
          Caption = 'Bulb weight'
          Enabled = False
        end
        object Label10: TLabel
          Left = 0
          Top = 105
          Width = 53
          Height = 13
          Caption = 'Wing width'
        end
        object Label19: TLabel
          Left = 0
          Top = 129
          Width = 67
          Height = 13
          Caption = 'Bulb'#39's material'
          Enabled = False
        end
        object InputShape: TComboBox
          Left = 102
          Top = 1
          Width = 125
          Height = 21
          Style = csDropDownList
          ItemHeight = 13
          ItemIndex = 0
          TabOrder = 0
          Text = 'No bullb'
          OnClick = InputShapeClick
          Items.Strings = (
            'No bullb'
            'Round profile'
            'Wing'
            'Round wing')
        end
        object InputWing: TComboBox
          Left = 102
          Top = 25
          Width = 125
          Height = 21
          Style = csDropDownList
          ItemHeight = 13
          ItemIndex = 37
          TabOrder = 1
          Text = 'NACA63-006'
          OnClick = InputWingClick
          Items.Strings = (
        'NACA63-006'
        'NACA63-009'
        'NACA63-010'
        'NACA63-012'
        'NACA63-015'
        'NACA63-018'
        'NACA63-021'
        'NACA63-010a'
        'NACA63-012a'
        'NACA63-015a'
        'NACA64-006'
        'NACA64-008'
        'NACA64-009'
        'NACA64-010'
        'NACA64-012'
        'NACA64-015'
        'NACA64-018'
        'NACA64-021'
        'NACA65-006'
        'NACA65-008'
        'NACA65-009'
        'NACA65-010'
        'NACA65-012'
        'NACA65-015'
        'NACA65-018'
        'NACA65-021'
        'NACA66-009'
        'NACA66-010'
        'NACA66-018'
        'NACA66-021'
        'NACA0006'
        'NACA0008'
        'NACA0009'
        'NACA0010'
        'NACA0012'
        'NACA0015'
        'NACA0018'
        'NACA0020'
        'NACA0021'
        'NACA0025'
        'NACA0030'
        'NACA0035'
        'Jouk-06'
        'Jouk-08'
        'Jouk-09'
        'Jouk-10'
        'Jouk-12'
        'Jouk-15'
        'Jouk-18'
        'Jouk-20'
        'Jouk-21'
        'Jouk-25'
        'Jouk-30'
        'Jouk-35'
        'E168'
        'E169'
        'E171(Dicke 12.28%)'
        'E297'
        'E472'
        'E473'
        'E474'
        'E475'
        'E476'
        'E477'
        'E478'
        'E479'
        'E520'
        'E521'
        'E836'
        'E837'
        'E838'
        'E862 STRUT'
        'E864 STRUT'
        'EA 6(-1)-009'
        'EA 6(-1)-012'
        'EA 6(-1)-006'
        'GOE 409'
        'GOE 410'
        'GOE 411'
        'GOE 443'
        'GOE 444'
        'GOE 445'
        'GOE 459'
        'GOE 460'
        'GOE 775'
        'GOE 776'
        'RAF 27'
        'RAF 30'
        'RAF 30 MOD'
        'S1010 HPV'
        'S1012'
        'S1014'
        'S1016'
        'S1046 17%'
        'S1048 14%'
        'S8035 14%'
        'SD8020-010-88'
        'NACA-M1'
        'NACA-M2'
        'NACA-M3'
        'NACA-0009sm'
        'NACA-0012'
        'NACA64-008a'
        'NACA64-010a'
        'NACA64-012a'
        'NACA642-015'
        'NACA642-015a'
        'NACA/LANGLEY LS(1)'
        'NACA/LANGLEY 64-012'
        'NACA/LANGLEY SC'
        'HQ 0/7'
        'HQ 0/9'
        'HQ 0/10'
        'WORTMANN FX 71-089A'
        'FX 71-120'
        'FX 71-L-150/20'
        'FX 71-L-150/25'
        'FX 71-L-150/K30'
        'FX LIII-142 K25'
        'FX L V-152'
        'BELL 540'
        'RYAN BQM-34'
        'EH,0.0/9.0'
        'Griffith 30%'
        'JOUKOWSKI 12%'
)
        end
        object InputLength: TFreeNumInput
          Left = 162
          Top = 49
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 1.200000000000000000
          Validate = False
          OnAfterSetValue = InputLengthAfterSetValue
          TabOrder = 2
        end
        object InputPoints: TFreeNumInput
          Left = 162
          Top = 193
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 0
          Digits = 12
          DataType = dtInteger
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 26.000000000000000000
          Validate = False
          TabOrder = 10
        end
        object InputDensity: TFreeNumInput
          Left = 162
          Top = 145
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 2
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 11.350000000000000000
          Validate = False
          Enabled = False
          OnAfterSetValue = InputDensityAfterSetValue
          TabOrder = 7
        end
        object Button1: TButton
          Left = 102
          Top = 167
          Width = 58
          Height = 25
          Caption = 'Calc'
          Enabled = False
          TabOrder = 3
        end
        object InputWeight: TFreeNumInput
          Left = 162
          Top = 169
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 0.600000000000000000
          Validate = False
          Enabled = False
          TabOrder = 8
        end
        object TrackBar2: TTrackBar
          Left = 118
          Top = 215
          Width = 115
          Height = 23
          Max = 20
          TabOrder = 11
          ThumbLength = 15
          OnChange = TrackBar1Change
        end
        object InputDelta: TFreeNumInput
          Left = 162
          Top = 73
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 0.100000000000000000
          Validate = False
          OnAfterSetValue = InputDeltaAfterSetValue
          TabOrder = 4
        end
        object Button2: TButton
          Left = 102
          Top = 47
          Width = 58
          Height = 25
          Caption = 'Calc'
          Enabled = False
          TabOrder = 9
        end
        object InputWingWidth: TFreeNumInput
          Left = 162
          Top = 97
          Width = 65
          Height = 21
          Alignment = taRightJustify
          Decimals = 3
          Digits = 12
          OutOfRangeMessage = False
          TabOnEnterKey = False
          Value = 0.100000000000000000
          Validate = False
          OnAfterSetValue = InputDeltaAfterSetValue
          TabOrder = 5
        end
        object InputMaterial: TComboBox
          Left = 103
          Top = 120
          Width = 125
          Height = 21
          Style = csDropDownList
          Enabled = True
          ItemHeight = 13
          ItemIndex = 0
          TabOrder = 6
          Text = 'Plumbum'
          OnChange = InputMaterialChange
          Items.Strings = (
            'Plumbum'
            'Steel'
            'Wolfram'
            'Set density')
        end
      end
    end
  end
  object Panel1: TPanel
    Left = 0
    Top = 0
    Width = 720
    Height = 22
    Align = alTop
    BevelOuter = bvNone
    TabOrder = 1
    object BitBtn1: TSpeedButton
      Left = 590
      Top = 1
      Width = 60
      Height = 19
      Caption = 'OK'
      Flat = True
      Glyph.Data = {
        DE010000424DDE01000000000000760000002800000024000000120000000100
        0400000000006801000000000000000000001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        3333333333333333333333330000333333333333333333333333F33333333333
        00003333344333333333333333388F3333333333000033334224333333333333
        338338F3333333330000333422224333333333333833338F3333333300003342
        222224333333333383333338F3333333000034222A22224333333338F338F333
        8F33333300003222A3A2224333333338F3838F338F33333300003A2A333A2224
        33333338F83338F338F33333000033A33333A222433333338333338F338F3333
        0000333333333A222433333333333338F338F33300003333333333A222433333
        333333338F338F33000033333333333A222433333333333338F338F300003333
        33333333A222433333333333338F338F00003333333333333A22433333333333
        3338F38F000033333333333333A223333333333333338F830000333333333333
        333A333333333333333338330000333333333333333333333333333333333333
        0000}
      NumGlyphs = 2
      OnClick = BitBtn1Click
    end
    object BitBtn2: TSpeedButton
      Left = 650
      Top = 1
      Width = 60
      Height = 19
      Caption = 'Cancel'
      Flat = True
      Glyph.Data = {
        DE010000424DDE01000000000000760000002800000024000000120000000100
        0400000000006801000000000000000000001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        333333333333333333333333000033338833333333333333333F333333333333
        0000333911833333983333333388F333333F3333000033391118333911833333
        38F38F333F88F33300003339111183911118333338F338F3F8338F3300003333
        911118111118333338F3338F833338F3000033333911111111833333338F3338
        3333F8330000333333911111183333333338F333333F83330000333333311111
        8333333333338F3333383333000033333339111183333333333338F333833333
        00003333339111118333333333333833338F3333000033333911181118333333
        33338333338F333300003333911183911183333333383338F338F33300003333
        9118333911183333338F33838F338F33000033333913333391113333338FF833
        38F338F300003333333333333919333333388333338FFF830000333333333333
        3333333333333333333888330000333333333333333333333333333333333333
        0000}
      NumGlyphs = 2
      OnClick = BitBtn2Click
    end
    object SpeedButton1: TSpeedButton
      Left = 16
      Top = 1
      Width = 65
      Height = 19
      Caption = 'Send'
      Flat = True
      OnClick = SpeedButton1Click
    end
    object SpeedButton2: TSpeedButton
      Left = 84
      Top = 1
      Width = 65
      Height = 19
      Caption = 'Save as part'
      Flat = True
      OnClick = SpeedButton2Click
    end
  end
  object Panel3: TPanel
    Left = 0
    Top = 475
    Width = 720
    Height = 13
    Align = alBottom
    BevelOuter = bvNone
    TabOrder = 2
  end
end
