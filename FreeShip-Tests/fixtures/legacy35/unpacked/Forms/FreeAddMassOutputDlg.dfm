object FreeAddMassOutputDialog: TFreeAddMassOutputDialog
  Left = 540
  Top = 90
  Width = 744
  Height = 539
  Caption = 'Add_Mass file generator.'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poMainFormCenter
  OnResize = FormResize
  PixelsPerInch = 96
  TextHeight = 13
  object Panel1: TPanel
    Left = 0
    Top = 0
    Width = 466
    Height = 22
    Align = alTop
    BevelOuter = bvNone
    TabOrder = 0
    object OKbutton: TSpeedButton
      Left = 330
      Top = 1
      Width = 65
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
      OnClick = OKbuttonClick
    end
    object CancelButton: TSpeedButton
      Left = 395
      Top = 1
      Width = 65
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
      OnClick = CancelButtonClick
    end
  end
  object Panel2: TPanel
    Left = 0
    Top = 22
    Width = 466
    Height = 477
    Align = alClient
    BevelOuter = bvLowered
    TabOrder = 1
    object PageControl1: TPageControl
      Left = 1
      Top = 1
      Width = 464
      Height = 475
      ActivePage = TabSheet1
      Align = alClient
      TabOrder = 0
      object TabSheet1: TTabSheet
        Caption = 'Hull geometry'
        object Panel4: TPanel
          Left = 0
          Top = 0
          Width = 456
          Height = 135
          Align = alTop
          BevelOuter = bvLowered
          TabOrder = 0
          object Label1: TLabel
            Left = 12
            Top = 16
            Width = 118
            Height = 13
            Caption = 'Number of stations (5-45)'
          end
          object Label2: TLabel
            Left = 12
            Top = 40
            Width = 129
            Height = 13
            Caption = 'Number of waterlines (5-25)'
          end
          object Label3: TLabel
            Left = 12
            Top = 64
            Width = 23
            Height = 13
            Caption = 'Draft'
          end
          object Label4: TLabel
            Left = 12
            Top = 88
            Width = 33
            Height = 13
            Caption = 'Length'
          end
          object Label5: TLabel
            Left = 12
            Top = 112
            Width = 35
            Height = 13
            Caption = 'Volume'
          end
          object _Label18: TLabel
            Left = 216
            Top = 64
            Width = 14
            Height = 13
            Caption = '[m]'
          end
          object _Label19: TLabel
            Left = 216
            Top = 88
            Width = 14
            Height = 13
            Caption = '[m]'
          end
          object _Label20: TLabel
            Left = 216
            Top = 112
            Width = 25
            Height = 13
            Caption = '[m^3]'
          end
          object Edit1: TFreeNumInput
            Left = 152
            Top = 8
            Width = 61
            Height = 21
            Alignment = taRightJustify
            Decimals = 0
            Digits = 12
            Max = 45.000000000000000000
            Min = 5.000000000000000000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 5.000000000000000000
            Validate = True
            OnAfterSetValue = Edit1AfterSetValue
            TabOrder = 0
          end
          object Edit2: TFreeNumInput
            Left = 152
            Top = 32
            Width = 61
            Height = 21
            Alignment = taRightJustify
            Decimals = 0
            Digits = 12
            Max = 25.000000000000000000
            Min = 5.000000000000000000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 5.000000000000000000
            Validate = True
            OnAfterSetValue = Edit2AfterSetValue
            TabOrder = 1
          end
          object Edit3: TFreeNumInput
            Left = 152
            Top = 56
            Width = 61
            Height = 21
            Alignment = taRightJustify
            Decimals = 3
            Digits = 12
            Max = 10000000000.000000000000000000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Validate = True
            OnAfterSetValue = Edit3AfterSetValue
            TabOrder = 2
          end
          object Edit4: TFreeNumInput
            Left = 152
            Top = 80
            Width = 61
            Height = 21
            Alignment = taRightJustify
            Decimals = 3
            Digits = 12
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Validate = False
            OnAfterSetValue = Edit4AfterSetValue
            Color = clBtnFace
            Font.Charset = DEFAULT_CHARSET
            Font.Color = clWindowText
            Font.Height = -11
            Font.Name = 'MS Sans Serif'
            Font.Style = []
            ParentFont = False
            ReadOnly = True
            TabOrder = 3
          end
          object Edit5: TFreeNumInput
            Left = 152
            Top = 104
            Width = 61
            Height = 21
            Alignment = taRightJustify
            Decimals = 3
            Digits = 12
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Validate = False
            OnAfterSetValue = Edit5AfterSetValue
            Color = clBtnFace
            Font.Charset = DEFAULT_CHARSET
            Font.Color = clWindowText
            Font.Height = -11
            Font.Name = 'MS Sans Serif'
            Font.Style = []
            ParentFont = False
            ReadOnly = True
            TabOrder = 4
          end
          object GroupBox3: TGroupBox
            Left = 264
            Top = 8
            Width = 231
            Height = 121
            Caption = 'Hullform configuration:  '
            TabOrder = 5
            object Label14: TLabel
              Left = 5
              Top = 76
              Width = 158
              Height = 13
              Caption = 'Distance between center of hulls:'
            end
            object _Label15: TLabel
              Left = 95
              Top = 100
              Width = 14
              Height = 13
              Caption = '[m]'
            end
            object RadioButton1: TRadioButton
              Left = 5
              Top = 16
              Width = 100
              Height = 17
              Caption = 'Monohull'
              Checked = True
              TabOrder = 0
              TabStop = True
              OnClick = RadioButton1Click
            end
            object RadioButton2: TRadioButton
              Left = 5
              Top = 36
              Width = 195
              Height = 17
              Caption = 'Monohull, send as catamaran'
              TabOrder = 1
              OnClick = RadioButton1Click
            end
            object RadioButton3: TRadioButton
              Left = 5
              Top = 56
              Width = 100
              Height = 17
              Caption = 'Catamaran'
              TabOrder = 2
              OnClick = RadioButton1Click
            end
            object Distance: TFreeNumInput
              Left = 8
              Top = 92
              Width = 85
              Height = 21
              Alignment = taRightJustify
              Decimals = 3
              Digits = 12
              OutOfRangeMessage = False
              TabOnEnterKey = False
              Validate = False
              OnAfterSetValue = DistanceAfterSetValue
              Enabled = False
              TabOrder = 3
            end
          end
        end
        object Panel6: TPanel
          Left = 0
          Top = 135
          Width = 456
          Height = 312
          Align = alClient
          BevelOuter = bvLowered
          TabOrder = 1
          object ScrollBar1: TScrollBar
            Left = 1
            Top = 298
            Width = 454
            Height = 13
            Align = alBottom
            Max = 180
            Min = -180
            PageSize = 0
            Position = 20
            TabOrder = 0
          end
          object ScrollBar2: TScrollBar
            Left = 442
            Top = 1
            Width = 13
            Height = 297
            Align = alRight
            Kind = sbVertical
            Max = 180
            Min = -180
            PageSize = 0
            Position = 20
            TabOrder = 1
          end
          object Viewport: TFreeViewport
            Left = 1
            Top = 1
            Width = 441
            Height = 297
            Angle = 20.000000000000000000
            Align = alClient
            BackgroundImage.Alpha = 255
            BackgroundImage.Tolerance = 5
            BackgroundImage.Transparent = False
            BackgroundImage.TransparentColor = clBlack
            BackgroundImage.Visible = True
            CameraType = ftStandard
            Color = clBlack
            DoubleBuffer = True
            Elevation = 20.000000000000000000
            HorScrollbar = ScrollBar1
            VertScrollbar = ScrollBar2
            ViewType = fvPerspective
            ViewportMode = vmWireFrame
            OnRedraw = ViewportRedraw
            OnRequestExtents = ViewportRequestExtents
          end
        end
      end
      object TabSheet3: TTabSheet
        Caption = 'Environment'
        ImageIndex = 2
        object Panel7: TPanel
          Left = 0
          Top = 0
          Width = 456
          Height = 447
          Align = alClient
          BevelOuter = bvLowered
          TabOrder = 0
          object Label6: TLabel
            Left = 12
            Top = 16
            Width = 129
            Height = 13
            Caption = 'Water density'
          end
          object Label7: TLabel
            Left = 12
            Top = 40
            Width = 65
            Height = 13
            Caption = 'Water kinematic viscosity'
          end
          object _Label8: TLabel
            Left = 370
            Top = 40
            Width = 28
            Height = 13
            Caption = '*10-6  [m2/s]'
          end
          object _Label9: TLabel
            Left = 370
            Top = 16
            Width = 30
            Height = 13
            Caption = '[t/m3]'
          end
          object Label10: TLabel
            Left = 12
            Top = 64
            Width = 120
            Height = 13
            Caption = 'Water depth'
          end
          object _Label11: TLabel
            Left = 370
            Top = 64
            Width = 61
            Height = 13
            Caption = '[m]'
          end
          object Label12: TLabel
            Left = 12
            Top = 88
            Width = 59
            Height = 13
            Caption = 'Trim angle'
          end
          object _Label13: TLabel
            Left = 370
            Top = 88
            Width = 14
            Height = 13
            Caption = '[degr]'
          end
          object Label20: TLabel
            Left = 12
            Top = 112
            Width = 59
            Height = 13
            Caption = 'Number of areas'
          end
          object Label21: TLabel
            Left = 12
            Top = 136
            Width = 59
            Height = 13
            Caption = 'Percent of areas sum'
          end
          object _Label21: TLabel
            Left = 370
            Top = 136
            Width = 14
            Height = 13
            Caption = '[%]'
          end
          object Label22: TLabel
            Left = 12
            Top = 160
            Width = 59
            Height = 13
            Caption = 'Calculation accuracy'
          end
          object Label23: TLabel
            Left = 12
            Top = 184
            Width = 59
            Height = 13
            Caption = 'Type of areas (1-triangle;2-trapezium)'
          end
          object Label24: TLabel
            Left = 12
            Top = 232
            Width = 59
            Height = 13
            Caption = 'Drift angle BetaD, degr'
          end
          object Label25: TLabel
            Left = 12
            Top = 256
            Width = 59
            Height = 13
            Caption = 'Estimated velocity Vs, knots'
          end
          object Label26: TLabel
            Left = 12
            Top = 280
            Width = 59
            Height = 13
            Caption = 'Rotative velocity Omega, 1/sec'
          end
          object Label27: TLabel
            Left = 12
            Top = 208
            Width = 59
            Height = 13
            Caption = 'Type of object or ship:'
          end
          object ComboBox: TComboBox
            Left = 168
            Top = 200
            Width = 200
            Height = 21
            ItemHeight = 13
            ItemIndex = 0
            TabOrder = 0
            Text = 'Type of object or ship:'
            OnClick = ComboBoxClick
            Items.Strings = (
             'Any Objects or Ships'
             'Underwater Vechicle'
             'Displacement Sea Ships'
             'Displacement River Ships')
          end
          object Edit6: TFreeNumInput
            Left = 290
            Top = 8
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 3
            Digits = 12
            Max = 1.030000000000000000
            Min = 0.995000000000000000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 1.000000000000000000
            Validate = False
            OnAfterSetValue = Edit6AfterSetValue
            TabOrder = 1
          end
          object Edit7: TFreeNumInput
            Left = 290
            Top = 32
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 4
            Digits = 12
            Max = 1.610000000000000000
            Min = 0.800000000000000000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 0.800000000000000000
            Validate = True
            Color = clBtnFace
            Font.Charset = DEFAULT_CHARSET
            Font.Color = clWindowText
            Font.Height = -11
            Font.Name = 'MS Sans Serif'
            Font.Style = []
            ParentFont = False
            OnAfterSetValue = Edit7AfterSetValue
            TabOrder = 2
          end
          object Edit8: TFreeNumInput
            Left = 290
            Top = 56
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 2
            Digits = 12
            Max = 10000.00
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Validate = True
            OnAfterSetValue = Edit8AfterSetValue
            TabOrder = 3
          end
          object Edit9: TFreeNumInput
            Left = 290
            Top = 80
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 3
            Digits = 12
            Max = 90.000
            Min = -90.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 0.0000000
            Validate = True
            OnAfterSetValue = Edit9AfterSetValue
            TabOrder = 0
          end
          object Edit20: TFreeNumInput
            Left = 290
            Top = 104
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 0
            Digits = 12
            Max = 4000.000
            Min = 400.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 1000.0000000
            Validate = True
            OnAfterSetValue = Edit20AfterSetValue
            TabOrder = 0
          end
          object Edit21: TFreeNumInput
            Left = 290
            Top = 128
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 0
            Digits = 12
            Max = 100.000
            Min = 10.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 100.0000000
            Validate = True
            OnAfterSetValue = Edit21AfterSetValue
            TabOrder = 0
          end
          object Edit22: TFreeNumInput
            Left = 290
            Top = 152
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 2
            Digits = 12
            Max = 1.000
            Min = 0.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 0.2
            Validate = True
            OnAfterSetValue = Edit22AfterSetValue
            TabOrder = 0
          end
          object Edit23: TFreeNumInput
            Left = 290
            Top = 176
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 0
            Digits = 12
            Max = 2.000
            Min = 1.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 1.000
            Validate = True
            OnAfterSetValue = Edit23AfterSetValue
            TabOrder = 0
          end
          object Edit24: TFreeNumInput
            Left = 290
            Top = 224
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 2
            Digits = 12
            Max = 90.000
            Min = 0.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 0.000
            Validate = True
            OnAfterSetValue = Edit24AfterSetValue
            TabOrder = 0
          end
          object Edit25: TFreeNumInput
            Left = 290
            Top = 248
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 2
            Digits = 12
            Max = 30.000
            Min = 0.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 10.000
            Validate = True
            OnAfterSetValue = Edit25AfterSetValue
            TabOrder = 0
          end
          object Edit26: TFreeNumInput
            Left = 290
            Top = 272
            Width = 78
            Height = 21
            Alignment = taRightJustify
            Decimals = 3
            Digits = 12
            Max = 30.000
            Min = 0.000
            OutOfRangeMessage = False
            TabOnEnterKey = False
            Value = 0.000
            Validate = True
            OnAfterSetValue = Edit26AfterSetValue
            TabOrder = 0
          end
        end
      end
    object TabSheet2: TTabSheet
      Caption = 'Results'
      ImageIndex = 1
      object Resultsmemo: TMemo
        Left = 0
        Top = 0
        Width = 735
        Height = 423
        Align = alClient
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clBlack
        Font.Height = -13
        Font.Name = 'Courier New'
        Font.Style = []
        ParentFont = False
        ScrollBars = ssBoth
        TabOrder = 0
        Visible = False
      end
    end
      object TabSheet4: TTabSheet
        Caption = 'Help'
        ImageIndex = 1
        object Resultsmemo2: TMemo
        Left = 0
        Top = 0
        Width = 735
        Height = 423
        Align = alClient
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clBlack
        Font.Height = -13
        Font.Name = 'Courier New'
        Font.Style = []
        ParentFont = False
        ScrollBars = ssBoth
        TabOrder = 0
        Visible = False
      end
    end
        end
      end
    end
  end
  object Panel3: TPanel
    Left = 0
    Top = 499
    Width = 466
    Height = 13
    Align = alBottom
    BevelOuter = bvNone
    TabOrder = 2
  end
  object ActionList1: TActionList
    Left = 557
    Top = 55
    object SaveFile: TAction
      Caption = 'Save'
    end
  end
end
