{#############################################################################################}
{    This code is distributed as part of the FREE!ship project. FREE!ship is an               }
{    open source surface-modelling program based on subdivision surfaces and intended for     }
{    designing ships.                                                                         }
{                                                                                             }
{    Copyright © 2005, by Martijn van Engeland                                                }
{    Copyright © 2007-2012, by Timoshenko V.F.                                                }
{    e-mail                  : vftim@rambler.ru                                               }
{    FREE!ship project page  : http://freeship-plus.pisem.su                                  }
{    FREE!ship homepage      : http://freeship-plus.pisem.su                                  }
{                                                                                             }
{    Copyright © 2015-2020, Conversion to FPC/Lazarus by Mark Malakanov.                      }
{                                                                                             }
{    This program is free software; you can redistribute it and/or modify it under            }
{    the terms of the GNU General Public License as published by the                          }
{    Free Software Foundation; either version 2 of the License, or (at your option)           }
{    any later version.                                                                       }
{                                                                                             }
{    This program is distributed in the hope that it will be useful, but WITHOUT ANY          }
{    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A          }
{    PARTICULAR PURPOSE. See the GNU General Public License for more details.                 }
{                                                                                             }
{    You should have received a copy of the GNU General Public License along with             }
{    this program; if not, write to the Free Software Foundation, Inc.,                       }
{    59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                                    }
{                                                                                             }
{#############################################################################################}

//{$DEFINE MEMCHECK}

unit Main;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

uses
{$IFNDEF LCL}
  jpeg, ShellAPI, Windows,
{$ELSE}
  {$IFDEF MSWINDOWS}Windows,{$ENDIF} LCLIntf, LCLType, LMessages,
{$ENDIF}

{$IFDEF VER3}
 LazUTF8,
 LazFileUtils,
{$ELSE}
 FileUtil, //deprecated
{$ENDIF}

     SysUtils,
     Variants,
     Classes,
     Graphics,
     IntfGraphics, GraphType, FPImage,
     Controls,
     Forms,
     Math,
     Dialogs,
     ExtCtrls,
     ActnList,
     StdCtrls,
     ImgList,
     ComCtrls,
     {$IFDEF MEMCHECK}
        MemCheck,     // Memcheck is used for memory-leak tracking (debugging all)
     {$ENDIF}
     FreeTypes,
     FreeGeometry,
     FreeShipUnit,
     FreeVersionUnit,
     freehullformwindow_panel, MDIPanel,
     FreeAboutDlg,
     FreeSplashWndw,
     Menus,
     Buttons, StdActns, Spin,
     LCLTranslator,
     FreeSplitSectionDlg,
     FreeLayerVisibilityDlg,
     FreeSelectedDlg
;

type

{ TMainForm }

 TMainForm         = class(TForm)
     AboutAction: TAction;
     ImportOBJ: TAction;
     ImportSTL: TAction;
     MenuImportSTL: TMenuItem;
     MenuItem3: TMenuItem;
     miSelectionDialog: TMenuItem;
     SelectionDialog: TAction;
     SplitSection50pct: TAction;
     miShowLayerVisibilityDialog: TMenuItem;
     LayerVisibilityDialog: TAction;
     miSetSplitSection: TMenuItem;
     SplitSectionDialog: TAction;
     miShowFreeObjects: TMenuItem;
     ShowFreeObjects: TAction;
     cbPrecision: TComboBox;
     miPointExtrude: TMenuItem;
     PointExtrude: TAction;
     MenuItem2: TMenuItem;
     PointsCoincide: TAction;
     AddGridPanel: TAction;
     AddFlowLine: TAction;
     LayerBox: TComboBox;
     MenuItem1: TMenuItem;
     miAddGridPanel: TMenuItem;
     SelectAllControlPoints: TAction;
     ColorButton1: TColorButton;
     SelectAllControlPoints1: TMenuItem;
     MenuItemPointAnchor: TMenuItem;
     PointAnchor: TAction;
     PointAlighnPermanently: TAction;
     AddPointToGroup: TAction;
     FontDialog1: TFontDialog;
     LabelProgress: TLabel;
     LabelNumbers: TLabel;
     LabelDistance: TLabel;
     LabelUndoMemory: TLabel;
     MainClientPanel: TPanel;
     AddControlPointToGroup: TMenuItem;
     MenuItemPointAlignPermanently: TMenuItem;
     StatusPanel5: TPanel;
     PanelMain: TPanel;
     ProgressBarMain: TProgressBar;
     SelectLeakPoints: TAction;
    FreeShip                   : TFreeShip;
    ActionList1                : TActionList;
    LoadFile                   : TAction;
    SelectLeakPoints1: TMenuItem;
    MH: TMenuItem;
    OST: TMenuItem;
    PropellerRvrs: TMenuItem;
    RBHS: TMenuItem;
    SplashWindow: TFreeSplashWindow;
    SpinEditFontSize: TSpinEdit;
    StatusBar                  : TPanel;
    MenuImages                 : TImageList;
    ExitProgram                : TAction;
    ShowControlNet             : TAction;
    ShowInteriorEdges          : TAction;
    MainMenu1                  : TMainMenu;
    File1                      : TMenuItem;
    Open       : TMenuItem;
    ToolBarCurves: TToolBar;
    ToolBarFaces: TToolBar;
    ToolBarFile: TToolBar;
    ToolBarEdit: TToolBar;
    ToolBarEdges: TToolBar;
    ToolBarPoints: TToolBar;
    ToolBarVisibility: TToolBar;
    ToolBarLayers: TToolBar;
    tbShowFreeObjects: TToolButton;
    ToolButton1: TToolButton;
    ToolButton39: TToolButton;
    ToolButtonSelect: TToolButton;
    ToolButtonRedo: TToolButton;
    ToolButtonUndo: TToolButton;
    Visibility1                : TMenuItem;
    ShowControlNet1            : TMenuItem;
    ShowInteriorEdges1         : TMenuItem;
    Window1                    : TMenuItem;
    Cascade1                   : TMenuItem;
    Tile1                       : TMenuItem;
    NewWindow1                 : TMenuItem;
    NewWindow                  : TAction;
    TileWindow                 : TAction;
    CascadeWindow              : TAction;
    N1                         : TMenuItem;
    BothSides                  : TAction;
    Showbothsides1             : TMenuItem;
    FileSaveas                 : TAction;
    Save1                      : TMenuItem;
    LayerAutoGroup             : TAction;
    Layer1                     : TMenuItem;
    Autogroup1                 : TMenuItem;
    NewLayer                   : TAction;
    New1                       : TMenuItem;
    Delete: TAction;
    ToolButtonOpenFile: TToolButton;
    ToolButton3: TToolButton;
    ToolButton5: TToolButton;
    ToolButton6: TToolButton;
    ToolButton8: TToolButton;
    ToolButton9: TToolButton;
    ToolButton10: TToolButton;
    Edit1: TMenuItem;
    Point1: TMenuItem;
    Edge1: TMenuItem;
    Face1: TMenuItem;
    EdgeCollapse: TAction;
    ToolButton13: TToolButton;
    Collapse1: TMenuItem;
    ToolButtonDelete: TToolButton;
    Delete1: TMenuItem;
    NewEdge: TAction;
    ToolButton16: TToolButton;
    New2: TMenuItem;
    ImportFEF: TAction;
    ImportOff1: TMenuItem;
    EdgeCrease: TAction;
    ToolButton17: TToolButton;
    Crease1: TMenuItem;
    DeselectAll: TAction;
    Selection1: TMenuItem;
    Clearselection1: TMenuItem;
    PointCollapse: TAction;
    ToolButton18: TToolButton;
    PointCollapse1: TMenuItem;
    ColorDialog: TColorDialog;
    ActiveLayerColor: TAction;
    Activelayercolor1: TMenuItem;
    DeleteEmptyLayers: TAction;
    Deleteempty1: TMenuItem;
    LayerDialog: TAction;
    Deleteempty2: TMenuItem;
    NewModel: TAction;
    ToolButton20: TToolButton;
    New3: TMenuItem;
    ShowStations: TAction;
    ToolButton21: TToolButton;
    Stations1: TMenuItem;
    ShowButtocks: TAction;
    ToolButton22: TToolButton;
    ShowWaterlines: TAction;
    ToolButton23: TToolButton;
    Buttocks1: TMenuItem;
    Waterlines1: TMenuItem;
    NewFace: TAction;
    ToolButton24: TToolButton;
    New4: TMenuItem;
    IntersectionDialog: TAction;
    ToolButton26: TToolButton;
    EdgeExtrude: TAction;
    ToolButton27: TToolButton;
    Extrude1: TMenuItem;
    EdgeSplit: TAction;
    ToolButton29: TToolButton;
    Split1: TMenuItem;
    ExportFEF: TAction;
    ExportFEFfile1: TMenuItem;
    EditProjectSettings: TAction;
    Project1: TMenuItem;
    Projectsettings1: TMenuItem;
    CheckModel: TAction;
    ools1: TMenuItem;
    Analyzesurface1: TMenuItem;
    ToolButton30: TToolButton;
    ShowNormals: TAction;
    DesignHydrostatics: TAction;
    Calculations1: TMenuItem;
    Hydrostatics1: TMenuItem;
    Normals1: TMenuItem;
    ImportVRML: TAction;
    Export1: TMenuItem;
    VRML1: TMenuItem;
    Import1: TMenuItem;
    RemoveNegative: TAction;
    Removenegative1: TMenuItem;
    RotateModel: TAction;
    Rotatemodel1: TMenuItem;
    RotateModelM: TAction;
    Rotatemodel2: TMenuItem;
    ScaleModel: TAction;
    Scale3D1: TMenuItem;
    ShowGrid: TAction;
    ToolButton31: TToolButton;
    Analyzesurface2: TMenuItem;
    Undo: TAction;
    Undo1: TMenuItem;
    StatusPanel2: TPanel;
    HydrostaticsDialog: TAction;
    Hydrostatics2: TMenuItem;
    ExportObj: TAction;
    WavefrontfileObj1: TMenuItem;
    InvertFace: TAction;
    Invert1: TMenuItem;
    ToolButton32: TToolButton;
    ToolButton33: TToolButton;
    Preferences: TAction;
    Preferences1: TMenuItem;
    N2: TMenuItem;
    ImportBodyplan: TAction;
    ExportDXF3DPolylines: TAction;
    ExportDXFPolylines1: TMenuItem;
    ExportDXFFaces: TAction;
    DXF3Dfaces1: TMenuItem;
    ImportHullFile: TAction;
    Carlssonhulfile1: TMenuItem;
    ExportOffsets: TAction;
    Offsets1: TMenuItem;
    MoveModel: TAction;
    Deselectall2: TMenuItem;
    AddPoint: TAction;
    Add1: TMenuItem;
    Intersections1: TMenuItem;
    DevelopLayers: TAction;
    Developplates1: TMenuItem;
    ExportArchimedes: TAction;
    N3: TMenuItem;
    ArchimedesMB1: TMenuItem;
    ToolButton34: TToolButton;
    ShowLinesplan: TAction;
    Linesplan1: TMenuItem;
    ShowDiagonals: TAction;
    ToolButton35: TToolButton;
    Diagonals1: TMenuItem;
    RecentFiles: TMenuItem;
    N4: TMenuItem;
    ImportCarene: TAction;
    Carenefile1: TMenuItem;
    ShowMarkers: TAction;
    ToolButton36: TToolButton;
    Markers1: TMenuItem;
    DeleteMarkers: TAction;
    Deletemarkers1: TMenuItem;
    ImportSurface: TAction;
    Surface1: TMenuItem;
    Showcurvature: TAction;
    ToolButton37: TToolButton;
    IncreaseCurvatureScale: TAction;
    DecreaseCurvatureScale: TAction;
    N5: TMenuItem;
    Decrcurvaturescale1: TMenuItem;
    Incrcurvaturescale1: TMenuItem;
    FileSave: TAction;
    ToolButton38: TToolButton;
    Save2: TMenuItem;
    Curvature1: TMenuItem;
    ImportChines: TAction;
    Chines1: TMenuItem;
    Curve1: TMenuItem;
    ShowControlCurves: TAction;
    Controlcurves1: TMenuItem;
    ToolButton2: TToolButton;
    NewCurve: TAction;
    AddCurve1: TMenuItem;
    ToolButton40: TToolButton;
    ExportCoordinates: TAction;
    Coordinates1: TMenuItem;
    InsertPlane: TAction;
    InsertPlane1: TMenuItem;
    ToolButton41: TToolButton;
    PointsLock: TAction;
    PointsLock1: TMenuItem;
    PointsUnlock: TAction;
    Unlockpoints1: TMenuItem;
    ToolButton42: TToolButton;
    ToolButton43: TToolButton;
    PointsUnlockAll: TAction;
    Unlockallpoints1: TMenuItem;
    ToolButton44: TToolButton;
    Markers2: TMenuItem;
    ImportMarkers: TAction;
    Import2: TMenuItem;
    ExportMichlet: TAction;
    MichletCFD1: TMenuItem;
    ExportAddMass: TAction;
    AddMassCFD1: TMenuItem;
    ResistanceKaper: TAction;
    ResistancePlaning: TAction;
    Resistance1: TMenuItem;
    Kaper1: TMenuItem;
    ResistanceDelft: TAction;
    Delftyachtseries1: TMenuItem;
    ResistanceHoltr: TAction;
    ResistanceOST: TAction;
    ResistanceRBHS: TAction;
    ResistanceMH: TAction;
    ResistanceHollen: TAction;
    ResistanceOortmer: TAction;
    ResistanceFungLeib: TAction;
    Holtr1: TMenuItem;
    Hollen: TMenuItem;
    Oortmer: TMenuItem;
    FungLeib: TMenuItem;
    Planing1: TMenuItem;
    PropellerTask1: TMenuItem;
    PropTask4_: TAction;
    PropTask5_: TAction;
    PropTask1_: TAction;
    PropTask2_: TAction;
    PropTask3_: TAction;
    PropTask4: TMenuItem;
    PropTask5: TMenuItem;
    PropTask1: TMenuItem;
    PropTask2: TMenuItem;
    PropTask3: TMenuItem;
    PropTaskRvrs_: TAction;
    PropTaskRvrs: TMenuItem;
    HydrodynManeuv_: TAction;
    HydrodynManeuv: TMenuItem;
    HydrodynTask: TMenuItem;
    HydrodynTask1_: TAction;
    HydrodynTask2_: TAction;
    HydrodynTask3_: TAction;
    HydrodynTask4_: TAction;
    HydrodynTask1: TMenuItem;
    HydrodynTask2: TMenuItem;
    HydrodynTask3: TMenuItem;
    HydrodynTask4: TMenuItem;
    StatusPanel3: TPanel;
    PointAlign: TAction;
    ToolButton45: TToolButton;
    Projectline1: TMenuItem;
    ImportMichletWaves: TAction;
    N6: TMenuItem;
    ImportMichletWaves1: TMenuItem;
    ShowHydrostatics: TAction;
    ToolButton46: TToolButton;
    Hydrostaticdata1: TMenuItem;
    MirrorFace: TAction;
    ransform1: TMenuItem;
    MirrorFace1: TMenuItem;
    ExportDXF2DPolylines: TAction;
    DXF2DPolylines1: TMenuItem;
    StatusPanel4: TPanel;
    TransformLackenby: TAction;
    Lackenby1: TMenuItem;
    ExportIGES: TAction;
    IGES1: TMenuItem;
    ExportPart: TAction;
    Part1: TMenuItem;
    ImportPart: TAction;
    Part2: TMenuItem;
    LayerIntersection: TAction;
    ToolButton47: TToolButton;
    Saveas1: TMenuItem;
    KeelRudderWizard: TAction;
    Deleteempty3: TMenuItem;
    Redo: TAction;
    Archimedes1: TMenuItem;
    ClearUndo: TAction;
    N7: TMenuItem;
    Undohistory1: TMenuItem;
    Clear1: TMenuItem;
    ShowUndoHistory: TAction;
    Show1: TMenuItem;
    ImportPolyCad: TAction;
    PolyCad1: TMenuItem;
    RemoveUnusedPoints: TAction;
    Removeunusedpoints1: TMenuItem;
    ExportGHS: TAction;
    GHS1: TMenuItem;
    ExportPAM: TAction;
    PAM1: TMenuItem;	
    ShowFlowlines: TAction;
    Flowlines1: TMenuItem;
    ToolButton48: TToolButton;
    AddCylinder: TAction;
    AddCylinder1: TMenuItem;
    SelectAll: TAction;
    Selectall1: TMenuItem;
    ExportSTL: TAction;
    STL1: TMenuItem;
    CrossCurves: TAction;
    Crosscurves1: TMenuItem;
    //SelectLeakPoints: TAction;
    SelectionSeparator1: TMenuItem;
    Select_LeakPoints: TMenuItem;

    //FMDIChildList : TList;
    FMDIPanelManager: TMDIPanelManager;

    procedure AddFlowLineExecute(Sender: TObject);
    procedure AddGridPanelExecute(Sender: TObject);
    procedure AddPointToGroupExecute(Sender: TObject);
    procedure cbPrecisionChange(Sender: TObject);
    procedure ColorButton1Click(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure FormChangeBounds(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure FormResize(Sender: TObject);
    procedure FormWindowStateChange(Sender: TObject);
    procedure ImportOBJExecute(Sender: TObject);
    procedure ImportSTLExecute(Sender: TObject);
    procedure LoadFileExecute(Sender   : TObject);
    procedure ExitProgramExecute(Sender: TObject);
    procedure LayerBoxPanelClick(Sender: TObject);
    procedure PointExtrudeExecute(Sender: TObject);
    procedure PointsCoincideExecute(Sender: TObject);
    procedure SelectionDialogExecute(Sender: TObject);
    procedure SplitSectionDialogExecute(Sender: TObject);
    procedure ShowFreeObjectsExecute(Sender: TObject);
    procedure LayerVisibilityDialogExecute(Sender: TObject);
    function  ShowSplashWindow:TModalResult;
    procedure FormShow(Sender: TObject);
    procedure MainClientPanelClick(Sender: TObject);
    procedure MenuItem1Click(Sender: TObject);
    procedure PanelMainResize(Sender: TObject);
    procedure PointAnchorExecute(Sender: TObject);
    procedure SelectAllControlPoints1Click(Sender: TObject);
    procedure SelectAllControlPointsExecute(Sender: TObject);
    procedure ShowControlNetExecute(Sender: TObject);
    procedure ShowInteriorEdgesExecute(Sender: TObject);
    procedure NewWindowExecute(Sender: TObject);
    procedure SpinEditFontSizeChange(Sender: TObject);
    procedure TileWindowExecute(Sender: TObject);
    procedure CascadeWindowExecute(Sender: TObject);
    procedure BothSidesExecute(Sender: TObject);
    procedure FreeShipFileChanged(Sender: TObject);
    //procedure PrecisionBoxChange(Sender: TObject);
    procedure FileSaveasExecute(Sender: TObject);
    procedure LayerAutoGroupExecute(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure NewLayerExecute(Sender: TObject);
    procedure DeleteExecute(Sender: TObject);
    procedure EdgeCollapseExecute(Sender: TObject);
    procedure NewEdgeExecute(Sender: TObject);
    procedure ImportFEFExecute(Sender: TObject);
    procedure EdgeCreaseExecute(Sender: TObject);
    procedure DeselectAllExecute(Sender: TObject);
    procedure PointCollapseExecute(Sender: TObject);
    procedure LayerBoxChange(Sender: TObject);
    procedure PanelActiveLayerColorClick(Sender: TObject);
    procedure ActiveLayerColorExecute(Sender: TObject);
    procedure DeleteEmptyLayersExecute(Sender: TObject);
    procedure LayerDialogExecute(Sender: TObject);
    procedure NewModelExecute(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure ShowStationsExecute(Sender: TObject);
    procedure ShowButtocksExecute(Sender: TObject);
    procedure ShowWaterlinesExecute(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
    procedure NewFaceExecute(Sender: TObject);
    procedure IntersectionDialogExecute(Sender: TObject);
    procedure EdgeExtrudeExecute(Sender: TObject);
    procedure About1Click(Sender: TObject);
    procedure EdgeSplitExecute(Sender: TObject);
    procedure ExportFEFExecute(Sender: TObject);
    procedure EditProjectSettingsExecute(Sender: TObject);
    procedure CheckModelExecute(Sender: TObject);
    procedure ShowNormalsExecute(Sender: TObject);
    procedure DesignHydrostaticsExecute(Sender: TObject);
    procedure ImportVRMLExecute(Sender: TObject);
    procedure RemoveNegativeExecute(Sender: TObject);
    procedure RotateModelExecute(Sender: TObject);
    procedure RotateModelMExecute(Sender: TObject);
    procedure ScaleModelExecute(Sender: TObject);
    procedure ShowGridExecute(Sender: TObject);
    procedure ToolButtonSelectClick(Sender: TObject);
    procedure UndoExecute(Sender: TObject);
    procedure FreeShipUpdateUndoData(Sender: TObject);
    procedure HydrostaticsDialogExecute(Sender: TObject);
    procedure ExportObjExecute(Sender: TObject);
    procedure InvertFaceExecute(Sender: TObject);
    procedure PreferencesExecute(Sender: TObject);
    procedure ImportBodyplanExecute(Sender: TObject);
    procedure ExportDXF3DPolylinesExecute(Sender: TObject);
    procedure ExportDXFFacesExecute(Sender: TObject);
    procedure ImportHullFileExecute(Sender: TObject);
    procedure ExportOffsetsExecute(Sender: TObject);
    procedure MoveModelExecute(Sender: TObject);
    procedure AddPointExecute(Sender: TObject);
    procedure DevelopLayersExecute(Sender: TObject);
    procedure ExportArchimedesExecute(Sender: TObject);
    procedure ShowLinesplanExecute(Sender: TObject);
    procedure ShowDiagonalsExecute(Sender: TObject);
    procedure FreeShipUpdateRecentFileList(Sender: TObject);
    procedure ImportCareneExecute(Sender: TObject);
    procedure ShowMarkersExecute(Sender: TObject);
    procedure DeleteMarkersExecute(Sender: TObject);
    procedure ImportSurfaceExecute(Sender: TObject);
    procedure ShowcurvatureExecute(Sender: TObject);
    procedure IncreaseCurvatureScaleExecute(Sender: TObject);
    procedure DecreaseCurvatureScaleExecute(Sender: TObject);
    procedure FileSaveExecute(Sender: TObject);
    procedure ImportChinesExecute(Sender: TObject);
    procedure ShowControlCurvesExecute(Sender: TObject);
    procedure NewCurveExecute(Sender: TObject);
    procedure ExportCoordinatesExecute(Sender: TObject);
    procedure InsertPlaneExecute(Sender: TObject);
    procedure PointsLockExecute(Sender: TObject);
    procedure PointsUnlockExecute(Sender: TObject);
    procedure PointsUnlockAllExecute(Sender: TObject);
    procedure ImportMarkersExecute(Sender: TObject);
    procedure ExportMichletExecute(Sender: TObject);
    procedure ExportAddMassExecute(Sender: TObject);
    procedure ResistanceKaperExecute(Sender: TObject);
    procedure ResistanceDelftExecute(Sender: TObject);
    procedure ResistanceHoltrExecute(Sender: TObject);
    procedure ResistanceHollenExecute(Sender: TObject);
    procedure ResistanceOortmerExecute(Sender: TObject);
    procedure ResistanceFungLeibExecute(Sender: TObject);
    procedure ResistanceOSTExecute(Sender: TObject);
    procedure ResistanceRBHSExecute(Sender: TObject);
    procedure ResistanceMHExecute(Sender: TObject);
    procedure ResistancePlaningExecute(Sender: TObject);
    procedure PropTask1Execute(Sender: TObject);
    procedure PropTask2Execute(Sender: TObject);
    procedure PropTask3Execute(Sender: TObject);
    procedure PropTask4Execute(Sender: TObject);
    procedure PropTask5Execute(Sender: TObject);
    Procedure PropTaskRvrsExecute(Sender: TObject);
    Procedure HydrodynManeuvExecute(Sender: TObject);
    procedure HydrodynTask1Execute(Sender: TObject);
    procedure HydrodynTask2Execute(Sender: TObject);
    procedure HydrodynTask3Execute(Sender: TObject);
    procedure HydrodynTask4Execute(Sender: TObject);
    procedure FreeShipChangeCursorIncrement(Sender: TObject);
    procedure StatusPanel3Click(Sender: TObject);
    procedure PointAlignExecute(Sender: TObject);
    procedure PointAlighnPermanentlyExecute(Sender: TObject);
    procedure ImportMichletWavesExecute(Sender: TObject);
    procedure ShowHydrostaticsExecute(Sender: TObject);
    procedure MirrorFaceExecute(Sender: TObject);
    procedure ExportDXF2DPolylinesExecute(Sender: TObject);
    procedure FreeShipUpdateGeometryInfo(Sender: TObject);
    procedure TransformLackenbyExecute(Sender: TObject);
    procedure ExportIGESExecute(Sender: TObject);
    procedure ExportPartExecute(Sender: TObject);
    procedure ImportPartExecute(Sender: TObject);
    procedure LayerIntersectionExecute(Sender: TObject);
    procedure KeelRudderWizardExecute(Sender: TObject);
    procedure RedoExecute(Sender: TObject);
    procedure ClearUndoExecute(Sender: TObject);
    procedure ShowUndoHistoryExecute(Sender: TObject);
    procedure ImportPolyCadExecute(Sender: TObject);
    procedure RemoveUnusedPointsExecute(Sender: TObject);
    procedure ExportGHSExecute(Sender: TObject);
    procedure ExportPAMExecute(Sender: TObject);
    procedure ShowFlowlinesExecute(Sender: TObject);
    procedure AddCylinderExecute(Sender: TObject);
    procedure SelectAllExecute(Sender: TObject);
    procedure ExportSTLExecute(Sender: TObject);
    procedure CrossCurvesExecute(Sender: TObject);
    procedure SelectLeakPointsExecute(Sender: TObject);
    procedure LoadMostRecentFile;
    procedure LoadNamedFile(FileName:string);
    procedure dumpIcons;
    procedure LoadToolIcons;
    function  getToolbarControlsWidth(tb:TToolBar): integer;
    function  getAllToolbarsControlWidth: integer;
    procedure AlignAllToolbars;
    procedure SetAllActionsEnabled(val : boolean);
    procedure InitiallyLoadModel;
   private    { Private declarations }
      FStudioBar: TPanel;
      FNativeLanguageHotkey: boolean;
      FStudioAbout: TSpeedButton;
      FStudioImages: TImageList;
      FStudioMenu: TPopupMenu;
      FStudioMoveStep: TMenuItem;
      FQuickLanguagePoll: TTimer;
      FQuickLanguageHeld: Boolean;
      FStudioOverflowMenu: TPopupMenu;
      FStudioOverflow: TSpeedButton;
      FStudioCalculationsMenu: TPopupMenu;
      FStudioFeatureMenus:array[0..18] of TPopupMenu;
      FStudioButtons: array[0..18] of TSpeedButton;
      FStudioGroups: array[0..5] of TPanel;
      FStudioWindowButtons:array[0..3] of TSpeedButton;
      FStudioViewButtons: array[0..2] of TSpeedButton;
      FStudioResizing: boolean;
      FStudioClientWidth, FStudioClientHeight: integer;
      FAllToolbarsControlsWidth : integer;
      FToolBarFileControlsWidth : integer;
      FToolBarVisibilityControlsWidth : integer;
      FToolBarLayersControlsWidth : integer;
      FToolBarPointsControlsWidth : integer;
      FToolBarEditControlsWidth : integer;
      FToolBarEdgesControlsWidth : integer;
      FToolBarFacesControlsWidth : integer;
      FToolBarCurvesControlsWidth : integer;
      FActionListHull : TActionList;
      FDestroying: boolean;
      FSplitSectionDialog: TFreeSplitSectionDialog;


      procedure CreateStudioBar;
      procedure StudioBarResize(Sender: TObject);
      procedure StudioClientResize(Sender: TObject);
      procedure StudioWindowMotion(Sender: TObject);
      procedure StudioViewClick(Sender: TObject);
      procedure StudioMenuClick(Sender: TObject);
      procedure StudioCalculationsClick(Sender: TObject);
      procedure StudioFeatureClick(Sender:TObject);
      procedure StudioMenuShortcut(var Msg: TLMKey; var Handled: Boolean);
      procedure StudioKeyDownBefore(Sender: TObject; var Key: Word; Shift: TShiftState);
      procedure TraceLanguageShortcut(const EventText: string);
      procedure NativeLanguageActivated(Sender: TObject);
      procedure NativeLanguageDeactivated(Sender: TObject);
      procedure RefreshNativeLanguageHotkey;
      procedure WMQuickLanguage(var Msg: TLMessage); message $0312;
      procedure StudioOverflowClick(Sender: TObject);
      procedure StudioOverflowPopup(Sender: TObject);
      procedure StudioOverflowExecute(Sender: TObject);
      procedure StudioWindowToggle(Sender:TObject);
      procedure StudioWindowVisibility(Sender:TObject);
      procedure StudioWindowExpand(Sender:TObject);
      procedure StudioDismissViews(Sender:TObject;Msg:Cardinal);
      procedure UpdateStudioViewButtons;
      procedure DismissPointEditorOnInput(Sender: TObject; Msg: Cardinal);
      procedure FLoadRecentFile(sender:TObject);
      procedure FreeLayerVisibilityDialogChange(Sender: TObject);
      procedure FreeShipChangeLayerData(Sender: TObject);
      procedure FreeShipChangeActiveLayer(Sender: TObject;Layer: TFreeSubdivisionLayer);
      procedure OnSplitSectionLocationChange(Sender: TObject; aValue: TFloatType);
      procedure OnSelectItem(Sender:TObject);
      procedure OnChangeActiveControlPoint(Sender:TObject);
      procedure OnChangeActiveControlEdge(Sender:TObject);
      procedure OnChangeActiveControlFace(Sender:TObject);
      procedure OnChangeActiveControlCurve(Sender:TObject);

      procedure HullformWindowOnActivate(Sender:TObject);
      procedure HullformWindowOnDeactivate(Sender:TObject);
      procedure HullformWindowOnClose(Sender:TObject; var CloseAction: TCloseAction);

      procedure CreateWnd; override;
      procedure CloseHullWindows;
      procedure FOpenHullWindows;   // Creates 4 different views on the hullform
   public     { Public declarations }
      FFileName : string;
      FModelInitallyLoaded : boolean;
      {$IFDEF FPC}
      function  ActiveMDIChild: TFreeHullWindow; reintroduce;
      function  MDIChildCount: Integer; reintroduce;
      function  GetMDIChildren(AIndex: Integer): TFreeHullWindow; reintroduce;
      procedure AbandonMDIChildren(AIndex: Integer);
      procedure Tile;
      procedure Cascade;
      {$ENDIF}
      procedure CustomExceptionHandler(Sender: TObject; E: Exception);
      constructor Create(AOwner: TComponent); override;
      destructor Destroy; override;

      procedure SetCaption;
      procedure DismissViewsExcept(Target:TControl);
      procedure RefreshStudioLanguage;
      procedure StudioQuickLanguageClick(Sender: TObject);
      procedure PollQuickLanguage(Sender: TObject);
      procedure HandleQuickLanguageKeys(Down: Boolean);
      procedure UpdateMenu;
      procedure UpdateStudioModeButtons;
      procedure RecentFilesDialogActivate(Sender: TObject);
      procedure ShowRecentFilesDialog;
  end;

var MainForm: TMainForm;
var GShowSplash : boolean = true;

implementation

uses //FreeSplashWndw,
  FreeShip_26Icons,
     FreeLinesplanFrm,
     FreeControlPointFrm,
     FreeKeelWizardDlg,
     FreeStringsUnit,
     FreeUiChoices,
     FreeEmptyModelChooserDlg,
     RibbonToolBarMgr,
     TileDialog,
     FreePointGroupForm,
     FreeLogger;

{$IFnDEF FPC}
  {$R *.dfm}
{$ELSE}
  {$R *.lfm}
{$ENDIF}


resourcestring
  rsStudioMoveStep = 'Movement step...';
  rsStudioBodyView = 'Body plan';
  rsStudioProfileView = 'Profile';
  rsStudioPlanView = 'Plan view';
  rsStudioPerspectiveView = 'Perspective';
  rsStudioChecks = 'Model checks';
  rsStudioMenu = 'Menu';
  rsStudioMore = 'More';
  rsStudioAbout = 'About';
  rsStudioNew = 'New';
  rsStudioOpen = 'Open';
  rsStudioSave = 'Save';
  rsStudioUndo = 'Undo';
  rsStudioRedo = 'Redo';
  rsStudioDeselect = 'Deselect';
  rsStudioPoint = 'Point';
  rsStudioEdge = 'Edge';
  rsStudioFace = 'Face';
  rsStudioLayers = 'Layers';
  rsStudioControlNet = 'Control net';
  rsStudioZoomIn = 'Zoom in';
  rsStudioZoomOut = 'Zoom out';
  rsStudioFit = 'Fit';
  rsStudioBackground = 'Background';
  rsStudioSections = 'Sections';
  rsStudioLinesPlan = 'Lines plan';
  rsStudioHydrostatics = 'Hydrostatics';
  rsStudioCalculations = 'Calculations';

type
  { Keep the compact icon and text when the bound action updates itself. }
  TStudioSpeedButton = class(TSpeedButton)
  private
    FStudioImages: TCustomImageList;
    FStudioImageIndex: integer;
    FStudioCaption: string;
    FModeGroupIndex: integer;
  protected
    procedure ActionChange(Sender: TObject; CheckDefaults: Boolean); override;
  public
    procedure SetStudioPresentation(AImages: TCustomImageList; AIndex: integer;
      const ACaption: string);
    procedure SetStudioCaption(const ACaption: string);
    procedure SetModeGroup(AGroupIndex: integer);
  end;

procedure TStudioSpeedButton.ActionChange(Sender: TObject; CheckDefaults: Boolean);
begin
  inherited ActionChange(Sender, CheckDefaults);
  if FModeGroupIndex > 0 then
  begin
    GroupIndex := FModeGroupIndex;
    AllowAllUp := True;
  end;
  if FStudioImages <> nil then
  begin
    Images := FStudioImages;
    ImageIndex := FStudioImageIndex;
    Caption := FStudioCaption;
  end;
end;

procedure TStudioSpeedButton.SetStudioPresentation(AImages: TCustomImageList;
  AIndex: integer; const ACaption: string);
begin
  FStudioImages := AImages;
  FStudioImageIndex := AIndex;
  FStudioCaption := ACaption;
  Images := AImages;
  ImageIndex := AIndex;
  ImageWidth := 20;
  Caption := ACaption;
end;

procedure TStudioSpeedButton.SetStudioCaption(const ACaption: string);
begin
  FStudioCaption := ACaption;
  Caption := ACaption;
  Hint := ACaption;
end;

procedure TStudioSpeedButton.SetModeGroup(AGroupIndex: integer);
begin
  FModeGroupIndex := AGroupIndex;
  GroupIndex := AGroupIndex;
  AllowAllUp := True;
end;

// The toolbar preserves existing actions while giving each one a compact icon.
procedure TMainForm.CreateStudioBar;
const
  IconNames: array[0..24] of string = (
    'Menu', 'ProjectSettings', 'Observe', 'ModelCheck', 'Transform', 'Redo', 'Deselect', 'Point', 'Edge',
    'Face', 'Layers', 'ControlNet', 'ZoomIn', 'ZoomOut', 'Fit', 'Background',
    'Sections', 'LinesPlan', 'Calculate', 'More', 'Info',
    'ViewBody','ViewProfile','ViewPlan','ViewPerspective');
  IconSizes: array[0..3] of integer = (20, 25, 30, 40);
var GroupIndex, ButtonIndex: integer;
    Group: TPanel;
    Button: TStudioSpeedButton;
    IconIndexes: array[0..24] of integer;
    MenuItem: TMenuItem;
    Chinese: boolean;

  procedure LoadStudioImages;
  var I, J: integer;
      Images: array[0..3] of TCustomBitmap;
      IconPath, BasePath: string;
      Resolution: TCustomImageListResolution;
  begin
    FStudioImages := TImageList.Create(Self);
    FStudioImages.Name := 'StudioToolbarImages';
    FStudioImages.Width := 20;
    FStudioImages.Height := 20;
    FStudioImages.Scaled := True;
    for J := Low(IconSizes) to High(IconSizes) do
      Resolution := FStudioImages.Resolution[IconSizes[J]];
    BasePath := IncludeTrailingPathDelimiter(FreeShip.Preferences.GlobalAppDataDirectory) +
      'Themes' + DirectorySeparator + 'StudioLight' + DirectorySeparator + 'toolbar';
    for I := Low(IconNames) to High(IconNames) do
    begin
      IconIndexes[I] := -1;
      for J := Low(Images) to High(Images) do Images[J] := nil;
      try
        try
          for J := Low(Images) to High(Images) do
          begin
            IconPath := BasePath + DirectorySeparator + IntToStr(IconSizes[J]) +
              DirectorySeparator + IconNames[I] + '.png';
            if not HasFreeShip_26Icon(IconNames[I]) and not FileExistsUTF8(IconPath) then
              raise Exception.Create('Toolbar icon missing: ' + IconPath);
            Images[J] := TPortableNetworkGraphic.Create;
            if HasFreeShip_26Icon(IconNames[I]) then
              LoadFreeShip_26Icon(Images[J],IconNames[I],IconSizes[J])
            else Images[J].LoadFromFile(IconPath);
          end;
          IconIndexes[I] := FStudioImages.AddMultipleResolutions(Images);
        except
          on E: Exception do logger.Error(E.Message);
        end;
      finally
        for J := Low(Images) to High(Images) do Images[J].Free;
      end;
    end;
  end;

  function LabelText(const ZH, EN: string): string;
  begin
    if Chinese then Result := ZH else Result := EN;
  end;

  procedure AddGroup;
  begin
    Group := TPanel.Create(Self);
    FStudioGroups[GroupIndex] := Group;
    Group.Parent := FStudioBar;
    Group.Caption := '';
    Group.ParentBackground := False;
    Group.ParentColor := False;
    case GroupIndex of
      0: Group.Color := RGB(237, 244, 248);
      1: Group.Color := RGB(248, 243, 232);
      else Group.Color := RGB(229, 243, 237);
    end;
    Group.BevelOuter := bvNone;
    Group.Height := Scale96ToForm(52);
    ButtonIndex := 0;
    Inc(GroupIndex);
  end;

  procedure AddButton(IconIndex: integer; const ZH, EN: string; Action: TAction; ViewCommand: integer = -1);
  var ButtonWidth: integer;
  begin
    Button := TStudioSpeedButton.Create(Self);
    Button.Parent := Group;
    Button.Flat := True;
    Button.ShowHint := True;
    Button.ParentFont := True;
    if Assigned(Action) then Button.Action := Action
    else if ViewCommand >= 0 then
    begin
      Button.Tag := ViewCommand;
      Button.OnClick := StudioViewClick;
      FStudioViewButtons[ViewCommand] := Button;
    end
    else Button.OnClick := StudioMenuClick;
    Button.SetStudioPresentation(FStudioImages, IconIndexes[IconIndex], LabelText(ZH, EN));
    FStudioButtons[IconIndex] := Button;
    Button.Layout := blGlyphTop;
    Button.Spacing := Scale96ToForm(2);
    Button.Margin := Scale96ToForm(2);
    Button.Hint := Button.Caption;
    if (Action = ShowControlNet) or (Action = AddPoint) then
    begin
      if Action = ShowControlNet then Button.SetModeGroup(1)
      else Button.SetModeGroup(2);
      Button.Down := Action.Checked;
    end;
    if Chinese then ButtonWidth := Scale96ToForm(60)
    else ButtonWidth := Scale96ToForm(88);
    Button.SetBounds(ButtonIndex * ButtonWidth, Scale96ToForm(2),
      ButtonWidth - Scale96ToForm(2), Scale96ToForm(48));
    Inc(ButtonIndex);
    Group.Width := ButtonIndex * ButtonWidth;
  end;

  procedure Detach(Item:TMenuItem);
  begin if Assigned(Item.Parent) then Item.Parent.Remove(Item) end;

  procedure BindFeature(Index:Integer;Root:TMenuItem);
  var M:TPopupMenu;Child:TMenuItem;
  begin
    M:=TPopupMenu.Create(Self);M.Name:='StudioFeature'+IntToStr(Index);
    FStudioFeatureMenus[Index]:=M;
    Detach(Root);
    while Root.Count>0 do begin Child:=Root.Items[0];Root.Remove(Child);M.Items.Add(Child) end;
    FStudioButtons[Index].Tag:=Index;FStudioButtons[Index].OnClick:=StudioFeatureClick;
  end;

  procedure MoveToChecks(Item:TMenuItem);
  begin Detach(Item);FStudioFeatureMenus[3].Items.Add(Item) end;

  procedure RemoveDirectEntries(Root:TMenuItem);
  var I:Integer;Child:TMenuItem;
  begin
    for I:=Root.Count-1 downto 0 do begin
      Child:=Root.Items[I];RemoveDirectEntries(Child);
      if (Child.Action=ShowControlNet) or (Child.Action=ShowLinesplan) or
         (Child.Action=IntersectionDialog) or (Child.Action=EditProjectSettings) then Root.Remove(Child);
    end;
  end;

begin
  Chinese := SameText(FreeShip.Preferences.Language, 'Chinese');
  PanelMain.Visible := False;
  Color := RGB(246, 249, 247);
  MainClientPanel.ParentColor := False;
  MainClientPanel.Color := RGB(221, 231, 226);
  FStudioClientWidth := MainClientPanel.ClientWidth;
  FStudioClientHeight := MainClientPanel.ClientHeight;
  MainClientPanel.OnResize := StudioClientResize;
  StatusBar.ParentBackground := False;
  StatusBar.ParentColor := False;
  StatusBar.Color := RGB(250, 252, 250);
  StatusBar.BevelOuter := bvNone;
  StatusPanel2.ParentColor := True;
  StatusPanel3.ParentColor := True;
  StatusPanel4.ParentColor := True;
  StatusPanel5.ParentColor := True;
  StatusPanel2.BevelOuter := bvNone;
  StatusPanel3.BevelOuter := bvNone;
  StatusPanel4.BevelOuter := bvNone;
  StatusPanel5.BevelInner := bvNone;
  StatusPanel2.Visible := False;
  StatusPanel3.Visible := False;
  StatusPanel4.Visible := False;
  StatusBar.Visible := False;
  Font.Name := 'Microsoft YaHei UI';
  Font.Size := 9;
  Resistance1.Visible := False;
  PropellerTask1.Visible := False;
  PropellerRvrs.Visible := False;
  HydrodynTask.Visible := False;

  FStudioBar := TPanel.Create(Self);
  FStudioBar.Parent := Self;
  FStudioBar.Align := alTop;
  FStudioBar.BevelOuter := bvNone;
  FStudioBar.ParentBackground := False;
  FStudioBar.ParentColor := False;
  FStudioBar.Color := RGB(224, 236, 232);
  FStudioBar.Font.Name := 'Microsoft YaHei UI';
  FStudioBar.Font.Size := 9;
  FStudioBar.Name := 'StudioToolbar';
  FStudioBar.Caption := '';
  FStudioBar.Height := Scale96ToForm(58);
  FStudioBar.OnResize := StudioBarResize;
  LoadStudioImages;
  FStudioMenu := TPopupMenu.Create(Self);
  FStudioMenu.Name := 'StudioMainMenu';
  Self.Menu := nil;
  while MainMenu1.Items.Count > 0 do
  begin
    MenuItem := MainMenu1.Items[0];
    MainMenu1.Items.Remove(MenuItem);
    FStudioMenu.Items.Add(MenuItem);
  end;
  FStudioMoveStep := TMenuItem.Create(Self);
  FStudioMoveStep.Name := 'StudioMoveStep';
  FStudioMoveStep.OnClick := StatusPanel3Click;
  ools1.Add(FStudioMoveStep);
  Detach(Preferences1);
  FStudioMenu.Items.Add(Preferences1);
  OnShortcut := StudioMenuShortcut;
  GroupIndex := 0;
  AddGroup;
  Group.Name := 'StudioPrimaryGroup';
  Group.Caption := '';
  AddButton(0, '菜单', 'Menu', nil);
  AddButton(8, '边', 'Edge', nil);
  AddButton(9, '面', 'Face', nil);
  AddButton(7, '点', 'Point', nil);
  AddButton(10, '图层', 'Layers', nil);
  AddButton(2, '观察', 'Visibility', nil);
  AddButton(3, '检查', 'Model checks', nil);
  AddButton(4, '变换', 'Transform', nil);
  AddButton(18, '计算', 'Calculations', nil);
  Button.Name:='StudioCalculations'; Button.OnClick:=StudioCalculationsClick;
  FStudioCalculationsMenu:=TPopupMenu.Create(Self);
  FStudioCalculationsMenu.Name:='StudioCalculationsMenu';
  if Assigned(Calculations1.Parent) then Calculations1.Parent.Remove(Calculations1);
  while Calculations1.Count>0 do begin
    MenuItem:=Calculations1.Items[0]; Calculations1.Remove(MenuItem);
    FStudioCalculationsMenu.Items.Add(MenuItem);
  end;
  AddGroup;
  Group.Name := 'StudioProjectGroup';
  Group.Caption := '';
  AddButton(1, '设计船舶', 'Project settings', EditProjectSettings);
  AddButton(16, '剖面线', 'Sections', IntersectionDialog);
  AddButton(17, '型线图', 'Lines plan', ShowLinesplan);
  AddGroup;
  Group.Name := 'StudioModeGroup';
  Group.Caption := '';
  AddButton(11, '控制网格', 'Control net', ShowControlNet);
  AddGroup;
  Group.Name:='StudioViewsGroup';Group.Caption:='';Group.Color:=RGB(231,240,246);
  for GroupIndex:=0 to 3 do begin
    Button:=TStudioSpeedButton.Create(Self);Button.Parent:=Group;
    Button.Name:='StudioWindowToggle'+IntToStr(GroupIndex);Button.Tag:=GroupIndex;
    Button.Flat:=True;Button.ShowHint:=True;Button.ParentFont:=True;Button.Layout:=blGlyphTop;
    Button.Spacing:=Scale96ToForm(2);Button.Margin:=Scale96ToForm(2);
    Button.SetStudioPresentation(FStudioImages,IconIndexes[21+GroupIndex],'');
    Button.SetModeGroup(30+GroupIndex);Button.OnClick:=StudioWindowToggle;
    FStudioWindowButtons[GroupIndex]:=Button;
  end;
  FStudioButtons[11].Parent:=FStudioGroups[3];
  FStudioButtons[11].SendToBack;
  FreeAndNil(FStudioGroups[2]);
  BindFeature(7,Point1);BindFeature(8,Edge1);BindFeature(9,Face1);
  // Curve creation belongs with surface construction; move the real item once.
  Detach(Curve1);FStudioFeatureMenus[9].Items.Add(Curve1);
  BindFeature(10,Layer1);BindFeature(2,Visibility1);BindFeature(4,ransform1);
  FStudioFeatureMenus[3]:=TPopupMenu.Create(Self);FStudioFeatureMenus[3].Name:='StudioChecksMenu';
  FStudioButtons[3].Tag:=3;FStudioButtons[3].OnClick:=StudioFeatureClick;
  MoveToChecks(Analyzesurface1);MoveToChecks(SelectLeakPoints1);
  MoveToChecks(Removeunusedpoints1);MoveToChecks(Removenegative1);
  // Surface inspection and plate development have one route: viewport context.
  Detach(Normals1);Detach(Curvature1);Detach(Decrcurvaturescale1);Detach(Incrcurvaturescale1);
  Detach(Developplates1);Detach(N5);
  RemoveDirectEntries(FStudioMenu.Items);
  RemoveDirectEntries(FStudioCalculationsMenu.Items);
  RemoveDirectEntries(FStudioFeatureMenus[2].Items);
  Detach(Project1);Detach(Edit1);
  FStudioOverflowMenu := TPopupMenu.Create(Self);
  FStudioOverflowMenu.Name := 'StudioOverflowMenu';
  FStudioOverflowMenu.OnPopup := StudioOverflowPopup;
  Button := TStudioSpeedButton.Create(Self);
  Button.Name := 'StudioOverflow';
  Button.Parent := FStudioBar;
  Button.Flat := True;
  Button.ShowHint := True;
  Button.ParentFont := True;
  Button.Layout := blGlyphTop;
  Button.Spacing := Scale96ToForm(2);
  Button.Margin := Scale96ToForm(2);
  Button.SetStudioPresentation(FStudioImages, IconIndexes[19], LabelText('更多', 'More'));
  Button.Hint := Button.Caption;
  Button.OnClick := StudioOverflowClick;
  Button.SetBounds(0, 0, Scale96ToForm(60), Scale96ToForm(48));
  FStudioOverflow := Button;
  Button := TStudioSpeedButton.Create(Self);
  Button.Parent := FStudioBar;
  Button.Name := 'StudioAbout';
  Button.Action := AboutAction;
  Button.Flat := True; Button.ShowHint := True; Button.ParentFont := True;
  Button.Layout := blGlyphTop; Button.Spacing := Scale96ToForm(2);
  Button.Margin := Scale96ToForm(2);
  Button.SetStudioPresentation(FStudioImages, IconIndexes[20], rsStudioAbout);
  Button.Width := Scale96ToForm(70); FStudioAbout := Button;
  FStudioBar.BringToFront;
  RefreshStudioLanguage;
  UpdateStudioViewButtons;
  UpdateStudioModeButtons;
end;

procedure TMainForm.UpdateStudioModeButtons;
begin
  if not Assigned(FreeShip) then Exit;
  AddPoint.Checked := FreeShip.EditMode = emAddPoint;
  if Assigned(FStudioButtons[7]) then
  begin
    FStudioButtons[7].GroupIndex := 0;
    FStudioButtons[7].AllowAllUp := True;
    FStudioButtons[7].Down := False;
  end;
end;

procedure TMainForm.RefreshStudioLanguage;
var Labels: array[0..18] of string;
    I, J, LeftPos, ButtonWidth, MinWidth: integer;
    Group: TPanel;
    Button: TStudioSpeedButton;
    Measure: TBitmap;
  function WithoutEllipsis(const Text:string):string;
  begin
    Result:=TrimRight(Text);
    while (Length(Result)>=3) and (Copy(Result,Length(Result)-2,3)='...') do
      Result:=TrimRight(Copy(Result,1,Length(Result)-3));
    while (Length(Result)>=Length('…')) and
      (Copy(Result,Length(Result)-Length('…')+1,Length('…'))='…') do
      Result:=TrimRight(Copy(Result,1,Length(Result)-Length('…')));
  end;
  procedure CleanEntries(Owner:TComponent);
  var K:Integer;Item:TComponent;
  begin
    for K:=0 to Owner.ComponentCount-1 do begin
      Item:=Owner.Components[K];
      if Item is TAction then TAction(Item).Caption:=WithoutEllipsis(TAction(Item).Caption);
      if Item is TMenuItem then TMenuItem(Item).Caption:=WithoutEllipsis(TMenuItem(Item).Caption);
      CleanEntries(Item);
    end;
  end;
begin
  if not Assigned(FStudioBar) then Exit;
  CleanEntries(Self);
  RefreshNativeLanguageHotkey;
  if Assigned(FStudioAbout) then TStudioSpeedButton(FStudioAbout).SetStudioCaption(rsStudioAbout);
  if Assigned(FStudioMoveStep) then FStudioMoveStep.Caption := WithoutEllipsis(rsStudioMoveStep);
  SetChoiceLabels(cbPrecision, [rsChoiceLow, rsChoiceMedium, rsChoiceHigh, rsChoiceHighest]);
  TStudioSpeedButton(FStudioWindowButtons[0]).SetStudioCaption(rsStudioBodyView);
  TStudioSpeedButton(FStudioWindowButtons[1]).SetStudioCaption(rsStudioProfileView);
  TStudioSpeedButton(FStudioWindowButtons[2]).SetStudioCaption(rsStudioPlanView);
  TStudioSpeedButton(FStudioWindowButtons[3]).SetStudioCaption(rsStudioPerspectiveView);
  Labels[0] := rsStudioMenu;
  Labels[1] := EditProjectSettings.Caption; Labels[2] := Visibility1.Caption; Labels[3] := rsStudioChecks;
  Labels[4] := ransform1.Caption; Labels[5] := rsStudioRedo; Labels[6] := rsStudioDeselect;
  Labels[7] := rsStudioPoint; Labels[8] := rsStudioEdge; Labels[9] := rsStudioFace;
  Labels[10] := rsStudioLayers; Labels[11] := rsStudioControlNet;
  Labels[12] := rsStudioZoomIn; Labels[13] := rsStudioZoomOut; Labels[14] := rsStudioFit;
  Labels[15] := rsStudioBackground; Labels[16] := rsStudioSections;
  Labels[17] := rsStudioLinesPlan; Labels[18] := rsStudioCalculations;
  for I := Low(FStudioButtons) to High(FStudioButtons) do
    if Assigned(FStudioButtons[I]) then
      TStudioSpeedButton(FStudioButtons[I]).SetStudioCaption(Labels[I]);
  if Assigned(FStudioOverflow) then
    TStudioSpeedButton(FStudioOverflow).SetStudioCaption(rsStudioMore);

  Measure := TBitmap.Create;
  try
    Measure.SetSize(1, 1);
    Measure.Canvas.Font.Assign(FStudioBar.Font);
    MinWidth := Scale96ToForm(48);
    if Assigned(FStudioAbout) then begin
      Measure.Canvas.Font.Assign(FStudioAbout.Font);
      FStudioAbout.Width:=Max(MinWidth,Measure.Canvas.TextWidth(FStudioAbout.Caption)+Scale96ToForm(20));
    end;
    Measure.Canvas.Font.Assign(FStudioBar.Font);
    if Assigned(FreeShip.ControlpointForm) then FreeShip.ControlpointForm.RefreshPointCaption;
    if Assigned(FStudioOverflow) then
      FStudioOverflow.Width := Max(MinWidth,
        Measure.Canvas.TextWidth(FStudioOverflow.Caption) + Scale96ToForm(12));
    for I := Low(FStudioGroups) to High(FStudioGroups) do
      if Assigned(FStudioGroups[I]) then
      begin
        Group := FStudioGroups[I];
        LeftPos := 0;
        for J := 0 to Group.ControlCount - 1 do
          if Group.Controls[J] is TStudioSpeedButton then
          begin
            Button := TStudioSpeedButton(Group.Controls[J]);
            ButtonWidth := Max(MinWidth,
              Measure.Canvas.TextWidth(Button.Caption) + Scale96ToForm(12));
            if Group=FStudioGroups[3] then
              ButtonWidth:=Max(Scale96ToForm(48),
                Measure.Canvas.TextWidth(Button.Caption)+Scale96ToForm(12));
            Button.SetBounds(LeftPos, Scale96ToForm(2),
              ButtonWidth - Scale96ToForm(2), Scale96ToForm(48));
            Inc(LeftPos, ButtonWidth);
          end;
        Group.Width := LeftPos;
      end;
  finally
    Measure.Free;
  end;
  UpdateStudioViewButtons;
  StudioBarResize(FStudioBar);
end;

procedure TMainForm.StudioFeatureClick(Sender:TObject);
var Index:Integer;P:TPoint;
begin
 Index:=TControl(Sender).Tag;
 if (Index<0) or (Index>18) or not Assigned(FStudioFeatureMenus[Index]) then Exit;
 UpdateMenu;
 if Assigned(TControl(Sender).Parent) and (not TControl(Sender).Parent.Visible or not TControl(Sender).Visible) then
   P:=FStudioOverflow.ClientToScreen(Point(0,FStudioOverflow.Height))
 else P:=TControl(Sender).ClientToScreen(Point(0,TControl(Sender).Height));
 FStudioFeatureMenus[Index].PopUp(P.X,P.Y);
end;

procedure TMainForm.StudioCalculationsClick(Sender: TObject);
var P:TPoint;
begin
  if not Assigned(FStudioCalculationsMenu) then Exit;
  if not FStudioButtons[18].Parent.Visible or not FStudioButtons[18].Visible then
    P:=FStudioOverflow.ClientToScreen(Point(0,FStudioOverflow.Height))
  else P:=FStudioButtons[18].ClientToScreen(Point(0,FStudioButtons[18].Height));
  FStudioCalculationsMenu.PopUp(P.X,P.Y);
end;

procedure TMainForm.StudioMenuClick(Sender: TObject);
var P: TPoint;
begin
  if not Assigned(FStudioMenu) or not Assigned(FStudioButtons[0]) then Exit;
  P.X := 0; P.Y := FStudioButtons[0].Height;
  P := FStudioButtons[0].ClientToScreen(P);
  FStudioMenu.PopUp(P.X, P.Y);
end;

procedure TMainForm.StudioMenuShortcut(var Msg: TLMKey; var Handled: Boolean);
var I:Integer;
begin
  if (Msg.CharCode = Ord('M')) and (KeyDataToShiftState(Msg.KeyData) = [ssAlt]) then
  begin
    StudioMenuClick(FStudioButtons[0]);
    Handled := True;
  end
  else if Assigned(FStudioMenu) then
  begin
    Handled := FStudioMenu.IsShortCut(Msg);
    if not Handled and Assigned(FStudioCalculationsMenu) then
      Handled:=FStudioCalculationsMenu.IsShortCut(Msg);
    for I:=Low(FStudioFeatureMenus) to High(FStudioFeatureMenus) do
      if not Handled and Assigned(FStudioFeatureMenus[I]) then Handled:=FStudioFeatureMenus[I].IsShortCut(Msg);
  end;
end;

procedure TMainForm.NativeLanguageActivated(Sender: TObject);
begin RefreshNativeLanguageHotkey end;

procedure TMainForm.NativeLanguageDeactivated(Sender: TObject);
begin
  {$IFDEF MSWINDOWS}
  if FNativeLanguageHotkey then Windows.UnregisterHotKey(Handle,$5247);
  {$ENDIF}
  FNativeLanguageHotkey:=False;
end;

procedure TMainForm.RefreshNativeLanguageHotkey;
begin
  {$IFDEF MSWINDOWS}
  if not Assigned(FreeShip) then Exit;
  if not Application.Active or not FreeShip.Preferences.QuickLanguageEnabled or
     SameText(FreeShip.Preferences.QuickLanguageFirst,FreeShip.Preferences.QuickLanguageSecond) then
  begin NativeLanguageDeactivated(nil); Exit end;
  if FNativeLanguageHotkey then Exit;
  FNativeLanguageHotkey:=Windows.RegisterHotKey(Handle,$5247,$0001 or $0002 or $4000,Ord('L'));
  if FNativeLanguageHotkey then TraceLanguageShortcut('native-hotkey-registered')
  else TraceLanguageShortcut('native-hotkey-registration-failed error='+IntToStr(Windows.GetLastError));
  {$ENDIF}
end;

procedure TMainForm.WMQuickLanguage(var Msg: TLMessage);
begin
  if (Msg.WParam=$5247) and FNativeLanguageHotkey then begin
    TraceLanguageShortcut('native-hotkey-received');
    if not Assigned(Screen.GetCurrentModalForm) then HandleQuickLanguageKeys(True);
    Msg.Result:=0;
  end;
end;

procedure TMainForm.TraceLanguageShortcut(const EventText: string);
var Path, Line: string; Stream: TFileStream;
begin
  if not Assigned(FreeShip) then Exit;
  try
    Path:=GetTempDir(False)+'FreeShip_26-language-shortcut.log';
    Line:=FormatDateTime('yyyy-mm-dd hh:nn:ss.zzz',Now)+' '+EventText+
      ' enabled='+SysUtils.BoolToStr(FreeShip.Preferences.QuickLanguageEnabled,True)+
      ' language='+FreeShip.Preferences.Language+
      ' pair='+FreeShip.Preferences.QuickLanguageFirst+'/'+FreeShip.Preferences.QuickLanguageSecond+
      ' modal='+SysUtils.BoolToStr(Assigned(Screen.GetCurrentModalForm),True)+LineEnding;
    if FileExists(Path) then Stream:=TFileStream.Create(Path,fmOpenReadWrite or fmShareDenyNone)
    else Stream:=TFileStream.Create(Path,fmCreate);
    try Stream.Seek(0,soEnd); if Length(Line)>0 then Stream.WriteBuffer(Line[1],Length(Line));
    finally Stream.Free end;
  except
    // Diagnostics must never interrupt modeling or input handling.
  end;
end;

procedure TMainForm.StudioKeyDownBefore(Sender: TObject; var Key: Word; Shift: TShiftState);
var C:TControl;View:TFreeHullWindow;I:Integer;
begin
  // Resolve viewport shortcuts before popup/menu shortcuts or dialog-key handling.
  // Only this model's views participate; editors and other windows keep their keys.
  if (Shift=[ssCtrl]) and (Key in [Ord('A'),Ord('I'),Ord('O')]) and
     not Assigned(Screen.GetCurrentModalForm) and Assigned(FMDIPanelManager) then begin
    if Sender is TCustomEdit then begin
      if Key=Ord('A') then begin TCustomEdit(Sender).SelectAll;Key:=0 end;
      Exit;
    end;
    if Sender is TCustomComboBox then Exit;
    View:=nil;
    if Sender is TControl then begin
      C:=TControl(Sender);
      while Assigned(C) and (View=nil) do begin
        for I:=0 to MDIChildCount-1 do
          if (C=GetMDIChildren(I)) or (C=GetMDIChildren(I).Viewport) then begin View:=GetMDIChildren(I);Break end;
        C:=C.Parent;
      end;
    end;
    if (View=nil) and (Sender=Self) and
       not (Screen.ActiveControl is TCustomEdit) and
       not (Screen.ActiveControl is TCustomComboBox) then View:=ActiveMDIChild;
    if Assigned(View) then begin View.ViewportKeyDown(View.Viewport,Key,Shift);Exit end;
  end;
  if (Key=Ord('L')) and ((ssCtrl in Shift) or (ssAlt in Shift)) then
    TraceLanguageShortcut('key-before ctrl='+SysUtils.BoolToStr(ssCtrl in Shift,True)+
      ' alt='+SysUtils.BoolToStr(ssAlt in Shift,True)+' shift='+SysUtils.BoolToStr(ssShift in Shift,True));
  if (Key <> Ord('L')) or not (ssCtrl in Shift) or not (ssAlt in Shift) or
     (ssShift in Shift) or not Assigned(FreeShip) or
     not FreeShip.Preferences.QuickLanguageEnabled or
     SameText(FreeShip.Preferences.QuickLanguageFirst, FreeShip.Preferences.QuickLanguageSecond) or
     Assigned(Screen.GetCurrentModalForm) then Exit;
  if FNativeLanguageHotkey then Exit;
  Key := 0;
  HandleQuickLanguageKeys(True);
end;

procedure TMainForm.PollQuickLanguage(Sender: TObject);
var Down: Boolean;
begin
  Down:=False;
  {$IFDEF MSWINDOWS}
  if Application.Active and not Assigned(Screen.GetCurrentModalForm) then
    Down:=((Windows.GetAsyncKeyState(VK_CONTROL) and $8000)<>0)
      and ((Windows.GetAsyncKeyState(VK_MENU) and $8000)<>0)
      and ((Windows.GetAsyncKeyState(Ord('L')) and $8000)<>0)
      and ((Windows.GetAsyncKeyState(VK_SHIFT) and $8000)=0);
  {$ENDIF}
  HandleQuickLanguageKeys(Down);
end;

procedure TMainForm.HandleQuickLanguageKeys(Down: Boolean);
begin
  if Down and not FQuickLanguageHeld then
  begin
    FQuickLanguageHeld:=True;
    if Assigned(FreeShip) and FreeShip.Preferences.QuickLanguageEnabled and
       not Assigned(Screen.GetCurrentModalForm) then
    begin
      TraceLanguageShortcut('physical-key-press');
      StudioQuickLanguageClick(nil);
    end;
  end
  else if not Down then FQuickLanguageHeld:=False;
end;

procedure TMainForm.StudioQuickLanguageClick(Sender: TObject);
var Target: string; I: integer;
begin
  TraceLanguageShortcut('switch-request');
  if not FreeShip.Preferences.QuickLanguageEnabled then Exit;
  if SameText(FreeShip.Preferences.QuickLanguageFirst,
    FreeShip.Preferences.QuickLanguageSecond) then Exit;
  if SameText(FreeShip.Preferences.Language, FreeShip.Preferences.QuickLanguageFirst) then
    Target := FreeShip.Preferences.QuickLanguageSecond
  else
    Target := FreeShip.Preferences.QuickLanguageFirst;
  if not FreeShip.Preferences.TrySetInterfaceLanguage(Target) then
  begin
    MessageDlg(Format(rsQuickLanguageMissing, [LanguageDisplayName(Target)]), mtWarning, [mbOK], 0);
    Exit;
  end;
  TraceLanguageShortcut('switch-applied');
  RefreshStudioLanguage;
  SetCaption;
  if Assigned(FreeShip.ControlpointForm) then FreeShip.ControlpointForm.RefreshPointCaption;
  for I := 0 to MDIChildCount - 1 do
    if Assigned(GetMDIChildren(I)) then GetMDIChildren(I).SetCaption;
  FreeShipChangeCursorIncrement(nil);
  FreeShipUpdateGeometryInfo(nil);
  FreeShip.Redraw;
  try
    FreeShip.Preferences.SaveCurrentLanguage;
  except
    on E: Exception do MessageDlg(Format(rsQuickCannotSave, [E.Message]), mtWarning, [mbOK], 0);
  end;
end;

procedure TMainForm.StudioOverflowClick(Sender: TObject);
var P: TPoint;
begin
  if not Assigned(FStudioOverflowMenu) or not Assigned(FStudioOverflow) or not Assigned(FStudioAbout) then Exit;
  P.X := 0; P.Y := FStudioOverflow.Height;
  P := FStudioOverflow.ClientToScreen(P);
  FStudioOverflowMenu.PopUp(P.X, P.Y);
end;

procedure TMainForm.StudioOverflowPopup(Sender: TObject);
var I: integer;
    Button: TSpeedButton;
    Item: TMenuItem;
begin
  FStudioOverflowMenu.Items.Clear;
  for I:=0 to 3 do begin
    Button:=FStudioWindowButtons[I];
    if Assigned(Button) and not Button.Parent.Visible then begin
      Item:=TMenuItem.Create(FStudioOverflowMenu);Item.Caption:=Button.Caption;
      Item.Tag:=I;Item.Checked:=Button.Down;Item.Enabled:=Button.Enabled;
      Item.OnClick:=StudioWindowToggle;FStudioOverflowMenu.Items.Add(Item);
    end;
  end;
  for I := Low(FStudioButtons) to High(FStudioButtons) do
  begin
    Button := FStudioButtons[I];
    if Assigned(Button) and Assigned(Button.Parent) and (not Button.Parent.Visible or not Button.Visible) then
    begin
      Item := TMenuItem.Create(FStudioOverflowMenu);
      Item.Caption := Button.Caption;
      Item.Enabled := Button.Enabled;
      Item.Checked := Button.Down;
      Item.Tag := I;
      Item.OnClick := StudioOverflowExecute;
      FStudioOverflowMenu.Items.Add(Item);
    end;
  end;
end;

procedure TMainForm.StudioOverflowExecute(Sender: TObject);
var I: integer;
begin
  I := TMenuItem(Sender).Tag;
  if (I >= Low(FStudioButtons)) and (I <= High(FStudioButtons)) and
     Assigned(FStudioButtons[I]) and FStudioButtons[I].Enabled then
    FStudioButtons[I].Click;
end;

procedure TMainForm.StudioBarResize(Sender: TObject);
var I, J, X, Y, Gap, GroupGap, RowHeight, TotalWidth, Limit, NaturalWidth, Available, VisibleWidth, ViewGroupLeft: integer;
    NeedsOverflow, Hiding: boolean;
begin
  if FStudioResizing or not Assigned(FStudioBar) or not Assigned(FStudioOverflow) or not Assigned(FStudioAbout) then Exit;
  FStudioResizing := True;
  try
    Gap := Scale96ToForm(8);
    GroupGap := Scale96ToForm(40);
    if FStudioBar.ClientWidth<Scale96ToForm(1000) then GroupGap:=Scale96ToForm(16);
    // Recompute natural widths so a narrow layout can expand again.
    for I:=Low(FStudioGroups) to High(FStudioGroups) do if Assigned(FStudioGroups[I]) then begin
      NaturalWidth:=0;
      for J:=0 to FStudioGroups[I].ControlCount-1 do begin
        FStudioGroups[I].Controls[J].Visible:=True;
        NaturalWidth:=Max(NaturalWidth,FStudioGroups[I].Controls[J].Left+FStudioGroups[I].Controls[J].Width+Scale96ToForm(2));
      end;
      FStudioGroups[I].Width:=NaturalWidth;
    end;
    RowHeight := Scale96ToForm(52);
    X := Gap; Y := Scale96ToForm(3);
    TotalWidth := Gap;
    for I := Low(FStudioGroups) to High(FStudioGroups) do
      if Assigned(FStudioGroups[I]) then
        Inc(TotalWidth, FStudioGroups[I].Width + GroupGap);
    if TotalWidth + FStudioAbout.Width + Gap > FStudioBar.ClientWidth then
      GroupGap:=Scale96ToForm(16);
    NeedsOverflow := TotalWidth + FStudioAbout.Width + Gap > FStudioBar.ClientWidth;
    ViewGroupLeft:=FStudioBar.ClientWidth-Gap-FStudioAbout.Width-GroupGap-FStudioGroups[3].Width;
    Limit:=ViewGroupLeft;
    NeedsOverflow:=False;
    TotalWidth:=Gap;
    for I:=0 to 2 do if Assigned(FStudioGroups[I]) then Inc(TotalWidth,FStudioGroups[I].Width+GroupGap);
    NeedsOverflow:=TotalWidth>Limit;
    if NeedsOverflow then Dec(Limit, FStudioOverflow.Width + Gap);
    Hiding := False;
    for I := 0 to 2 do
      if Assigned(FStudioGroups[I]) then
      begin
        if (I > Low(FStudioGroups)) and (X + FStudioGroups[I].Width > Limit) then
          Hiding := True;
        FStudioGroups[I].Visible := not Hiding;
        if not Hiding then
        begin
          FStudioGroups[I].SetBounds(X, Y, FStudioGroups[I].Width, RowHeight);
          if (I=Low(FStudioGroups)) and (X+FStudioGroups[I].Width>Limit) then begin
            Available:=Limit-X;VisibleWidth:=0;
            for J:=0 to FStudioGroups[I].ControlCount-1 do begin
              with FStudioGroups[I].Controls[J] do
                Visible:=(J=0) or (Left+Width<=Available);
              if FStudioGroups[I].Controls[J].Visible then
                VisibleWidth:=Max(VisibleWidth,FStudioGroups[I].Controls[J].Left+FStudioGroups[I].Controls[J].Width+Scale96ToForm(2));
            end;
            FStudioGroups[I].Width:=VisibleWidth;Hiding:=True;
          end;
          Inc(X, FStudioGroups[I].Width + GroupGap);
        end;
      end;
    FStudioGroups[3].Visible:=True;
    if Hiding then Inc(X,FStudioOverflow.Width+Gap);
    FStudioGroups[3].SetBounds(X,Y,FStudioGroups[3].Width,RowHeight);
    FStudioAbout.SetBounds(FStudioBar.ClientWidth-FStudioAbout.Width-Gap, Y, FStudioAbout.Width, Scale96ToForm(48));
    FStudioOverflow.Visible := Hiding;
    if Hiding then
      FStudioOverflow.SetBounds(X-FStudioOverflow.Width-Gap, Y + Scale96ToForm(2), FStudioOverflow.Width,
        Scale96ToForm(48));
    FStudioBar.Height := Y + RowHeight + Scale96ToForm(2);
  finally
    FStudioResizing := False;
  end;
end;

// Keep the default four-up arrangement fitted, without retile of custom layouts.
procedure TMainForm.StudioWindowMotion(Sender:TObject);
var I:Integer; Moving:Boolean;
begin
  if FMDIPanelManager=nil then Exit;
  Moving:=False;
  for I:=0 to FMDIPanelManager.PanelCount-1 do
    Moving:=Moving or (FMDIPanelManager.MDIPanels[I].PositionState<>wpsNone);
  for I:=0 to FMDIPanelManager.PanelCount-1 do
    TFreeHullWindow(FMDIPanelManager.MDIPanels[I]).Viewport.PresentationFrozen:=Moving;
end;

procedure TMainForm.StudioClientResize(Sender: TObject);
var I, OldW, OldH, NewW, NewH: integer;
    Window, MinimizedWindow: TFreeHullWindow;
    Bounds: TRect;
    Tiled: boolean;
begin
  NewW := MainClientPanel.ClientWidth;
  NewH := MainClientPanel.ClientHeight;
  OldW := FStudioClientWidth;
  OldH := FStudioClientHeight;
  FStudioClientWidth := NewW;
  FStudioClientHeight := NewH;
  if not Assigned(FMDIPanelManager) or (NewW < 4) or (NewH < 4) then Exit;
  Tiled := (FMDIPanelManager.PanelCount = 4) and (OldW >= 4) and (OldH >= 4);
  if Tiled then
    for I := 0 to 3 do
    begin
      Window := TFreeHullWindow(FMDIPanelManager.MDIPanels[I]);
      if Window.WindowState = wsNormal then Bounds := Window.BoundsRect
      else Bounds := Window.NormalBounds;
      if (Bounds.Right - Bounds.Left <> OldW div 2) or
         (Bounds.Bottom - Bounds.Top <> OldH div 2) or
         ((Bounds.Left <> 0) and (Bounds.Left <> OldW div 2)) or
         ((Bounds.Top <> 0) and (Bounds.Top <> OldH div 2)) then
      begin Tiled := False; Break; end;
    end;
  MinimizedWindow := nil;
  for I := 0 to FMDIPanelManager.PanelCount-1 do
  begin
    Window := TFreeHullWindow(FMDIPanelManager.MDIPanels[I]);
    if Tiled then
    begin
      if Window.WindowState = wsNormal then Bounds := Window.BoundsRect
      else Bounds := Window.NormalBounds;
      if Bounds.Left > 0 then Bounds.Left := NewW div 2;
      if Bounds.Top > 0 then Bounds.Top := NewH div 2;
      Bounds.Right := Bounds.Left + NewW div 2;
      Bounds.Bottom := Bounds.Top + NewH div 2;
      if Window.WindowState = wsNormal then Window.BoundsRect := Bounds
      else Window.NormalBounds := Bounds;
    end;
    // State-specific bounds must follow the parent even for custom layouts.
    if Window.WindowState = wsMaximized then Window.SetBounds(0, 0, NewW, NewH)
    else if Window.WindowState = wsMinimized then MinimizedWindow := Window;
  end;
  if Assigned(MinimizedWindow) then MinimizedWindow.WindowState := wsMinimized;
end;

procedure TMainForm.StudioWindowToggle(Sender:TObject);
var Index:Integer;W:TFreeHullWindow;
begin
 if Sender is TControl then Index:=TControl(Sender).Tag else Index:=TMenuItem(Sender).Tag;
 if not Assigned(FMDIPanelManager) or (Index<0) or (Index>=MDIChildCount) then Exit;
 W:=GetMDIChildren(Index);
 if W.WindowState=wsMinimized then begin W.WindowState:=wsNormal;W.Show end
 else W.Visible:=not W.Visible;
 if W.Visible then W.BringToFront;
 UpdateStudioViewButtons;
end;
procedure TMainForm.StudioWindowVisibility(Sender:TObject);
var W:TFreeHullWindow;
begin
 if FDestroying then Exit;
 W:=TFreeHullWindow(Sender);W.Viewport.SetPresentationFrozen(not W.Visible);
 UpdateStudioViewButtons;
end;
procedure TMainForm.StudioWindowExpand(Sender:TObject);
var C:TControl;W:TFreeHullWindow;
begin
 if not (Sender is TControl) then Exit;
 C:=TControl(Sender);while Assigned(C) and not (C is TFreeHullWindow) do C:=C.Parent;
 if C=nil then Exit;W:=TFreeHullWindow(C);
 if W.WindowState=wsMaximized then W.WindowState:=wsNormal else W.WindowState:=wsMaximized;
 UpdateStudioViewButtons;
end;
procedure TMainForm.StudioDismissViews(Sender:TObject;Msg:Cardinal);
var C,Hit:TControl;I:Integer;W:TFreeHullWindow;
begin
 if FDestroying or not Assigned(FMDIPanelManager) or ((Msg<>LM_LBUTTONDOWN) and (Msg<>LM_RBUTTONDOWN) and (Msg<>LM_MBUTTONDOWN)) then Exit;
 if not (Sender is TControl) or (GetParentForm(TControl(Sender))<>Self) then Exit;
 Hit:=FindDragTarget(Mouse.CursorPos,True);
 if Assigned(Hit) and (GetParentForm(Hit)<>Self) then Hit:=nil;
 if not Assigned(Hit) then Hit:=TControl(Sender);
 DismissViewsExcept(Hit);
end;
procedure TMainForm.DismissViewsExcept(Target:TControl);
var C:TControl;I:Integer;W:TFreeHullWindow;
begin
 if FDestroying or not Assigned(FMDIPanelManager) then Exit;
 C:=Target;
 while Assigned(C) do begin
   if C=FStudioGroups[3] then Exit; // Toggle clicks must not hide/reopen their own target.
   C:=C.Parent;
 end;
 for I:=0 to MDIChildCount-1 do begin
   W:=GetMDIChildren(I);C:=Target;
   while Assigned(C) and (C<>W) do C:=C.Parent;
   if (C=nil) and W.Visible then W.Hide;
 end;
 UpdateStudioViewButtons;
end;
procedure TMainForm.UpdateStudioViewButtons;
var I: integer;
    Window: TFreeHullWindow;
begin
  for I:=0 to 3 do if Assigned(FStudioWindowButtons[I]) then begin
    FStudioWindowButtons[I].Enabled:=Assigned(FMDIPanelManager) and (I<MDIChildCount);
    FStudioWindowButtons[I].Down:=FStudioWindowButtons[I].Enabled and GetMDIChildren(I).Visible;
  end;
  Window := nil;
  if Assigned(FMDIPanelManager) then Window := ActiveMDIChild;
  for I := Low(FStudioViewButtons) to High(FStudioViewButtons) do
    if Assigned(FStudioViewButtons[I]) then
      FStudioViewButtons[I].Enabled := Assigned(Window);
end;

procedure TMainForm.StudioViewClick(Sender: TObject);
var Window: TFreeHullWindow;
begin
  if not Assigned(FMDIPanelManager) then Exit;
  Window := ActiveMDIChild;
  if Window = nil then Exit;
  case TSpeedButton(Sender).Tag of
    0: Window.ZoomIn.Execute;
    1: Window.ZoomOut.Execute;
    2: Window.ZoomExtents.Execute;
  end;
end;

procedure SortControlsByXY(tb:TWinControl);
var c1,c2: TControl; i,j :integer;
begin
  {
  Writeln();
  Writeln(tb.Name);
  for i:=0 to tb.ControlCount-1 do
   with tb.Controls[i] do
    Writeln( Name, ' t:',Top, ' l:', Left);
   }
  i:=0;
  while i <= tb.ControlCount-2 do
   begin
   j:=1;
   while j <= tb.ControlCount-1 do
      begin
      c1 := tb.Controls[j-1];
      c2 := tb.Controls[j];
      //t1:=c1.Top; t2:=c2.Top;
      //l1:=c1.Left; l2:=c2.Left;
      if c1.Top > c2.Top then
        begin
          tb.RemoveControl(c2);
          tb.InsertControl(c2,j-1);
        end
      else
        if (c1.Top = c2.Top) and (c1.Left > c2.Left) then
          begin
            tb.RemoveControl(c2);
            tb.InsertControl(c2,j-1);
          end
        else
          inc(j);
      end;
    inc(i);
    end;
 {
  Writeln();
  Writeln(tb.Name);
  for i:=0 to tb.ControlCount-1 do
   with tb.Controls[i] do
    Writeln( Name, ' t:',Top, ' l:', Left);
    }
end;


{$IFDEF MSWINDOWS}
function StudioDwmSetAttribute(Wnd:HWND;Attribute:DWORD;Value:Pointer;Size:DWORD):LongInt;stdcall;
  external 'dwmapi.dll' name 'DwmSetWindowAttribute';
{$ENDIF}
procedure TMainForm.CreateWnd;
{$IFDEF MSWINDOWS}var Tint:DWORD;{$ENDIF}
begin
  inherited CreateWnd;
  {$IFDEF MSWINDOWS}
  Tint:=RGB(224,236,232);
  StudioDwmSetAttribute(Handle,35,@Tint,SizeOf(Tint));
  StudioDwmSetAttribute(Handle,34,@Tint,SizeOf(Tint));
  Tint:=RGB(32,55,50);
  StudioDwmSetAttribute(Handle,36,@Tint,SizeOf(Tint));
  {$ENDIF}
end;

constructor TMainForm.Create(AOwner: TComponent);
begin
 Application.OnException := CustomExceptionHandler;
 inherited Create(AOwner);
 {$IFDEF WINDOWS}
 Icon.Assign(Application.Icon);
 {$ENDIF}
 FQuickLanguagePoll:=TTimer.Create(Self);
 FQuickLanguagePoll.Interval:=25;
 FQuickLanguagePoll.OnTimer:=PollQuickLanguage;
 // Outside-click hiding is intentionally disabled; retain the implementation for reuse.
 // Application.AddOnUserInputHandler(StudioDismissViews);
 Application.AddOnKeyDownBeforeHandler(StudioKeyDownBefore, True);
 Application.AddOnActivateHandler(NativeLanguageActivated);
 Application.AddOnDeactivateHandler(NativeLanguageDeactivated);
 FMDIPanelManager := TMDIPanelManager.Create;
 FFileName:='';
 TraceLanguageShortcut('diagnostic-start '+COMPILE_DATE+' '+COMPILE_TIME);

 //FreeShip.Preferences.SetDefaults;

{ SortControlsByXY(PanelMain);
 SortControlsByXY(ToolBarFile);
 SortControlsByXY(ToolBarVisibility);
 SortControlsByXY(ToolBarLayers);
 SortControlsByXY(ToolBarPoints);
 SortControlsByXY(ToolBarEdit);
 SortControlsByXY(ToolBarEdges);
 SortControlsByXY(ToolBarFaces);
 SortControlsByXY(ToolBarCurves);
 }

end;

procedure TMainForm.CustomExceptionHandler(Sender: TObject; E: Exception);
begin
  logger.ShowExceptionCallStack(E);
  Halt; // End of program execution
end;


destructor TMainForm.Destroy;
var i: Integer; thw:TFreeHullWindow;   ptr:pointer; al:TActionList;
begin
 if Assigned(FQuickLanguagePoll) then FQuickLanguagePoll.Enabled:=False;
 NativeLanguageDeactivated(nil);
 Application.RemoveOnActivateHandler(NativeLanguageActivated);
 Application.RemoveOnDeactivateHandler(NativeLanguageDeactivated);
 Application.RemoveOnKeyDownBeforeHandler(StudioKeyDownBefore);
 Application.RemoveOnUserInputHandler(DismissPointEditorOnInput);
 Application.RemoveOnUserInputHandler(StudioDismissViews);
 FDestroying := true;
 FreeShip.OnChangeActiveLayer:=nil;
 Freeship.OnChangeLayerData:=nil;
 //FreeShip.OnSelectItem:=nil;
 //FreeShip.OnChangeActiveControlPoint:=nil;
 FreeShip.Surface.RemoveOnChangeActiveControlPointListener(self.OnChangeActiveControlPoint);
 {al:=FActionListHull;
 ptr:=pointer(FActionListHull);
 if assigned(FActionListHull)
    then FreeAndNil(FActionListHull);}
 if assigned(FMDIPanelManager) then
 for i:=0 to FMDIPanelManager.PanelCount - 1 do
   if assigned(FMDIPanelManager.MDIPanels[i]) then
      begin
      thw:=TFreeHullWindow(FMDIPanelManager.MDIPanels[i]);
      //if assigned(thw.FreeHullForm) and assigned(thw.FreeHullForm.ActionListHull)
      //  then self.RemoveComponent(thw.FreeHullForm.ActionListHull);
      thw.Free;
      end;
 FreeAndNil(FMDIPanelManager);
 FreeAndNil(FreeShip);
 inherited;
end;

procedure TMainForm.FormChangeBounds(Sender: TObject);
begin
end;

procedure TMainForm.FormDestroy(Sender: TObject);
begin
end;

procedure TMainForm.FormResize(Sender: TObject);
begin
  //if MainForm.IsResizing then exit;
  //PanelMain.Invalidate;
  //Application.ProcessMessages;
  ///AlignAllToolbars;
end;

procedure TMainForm.FormWindowStateChange(Sender: TObject);
begin
 // this is just to kick toolbar to autoresize
{ Self.Height:=Self.Height+1;
 Self.Resize;
 Self.Height:=Self.Height-1;
 Self.Resize;
 Self.Invalidate;
 }
end;

procedure TMainForm.ImportOBJExecute(Sender: TObject);
begin
  Freeship.Edit.File_ImportOBJ;
  FOpenHullWindows;
  UpdateMenu;
end;


// total width of all controls in toolbar
function TMainForm.getToolbarControlsWidth(tb:TToolBar): integer;
var i:integer; c: TToolButton;
begin
  result := 0;
  for i:=0 to tb.ButtonCount-1 do
    begin
      c := tb.Buttons[i];
      result := result + c.Width + c.BorderSpacing.Right+c.BorderSpacing.Left+c.BorderSpacing.Around;
    end;
end;

function TMainForm.getAllToolbarsControlWidth: integer;
begin
  FToolBarFileControlsWidth := getToolbarControlsWidth(ToolBarFile);
  FToolBarVisibilityControlsWidth := getToolbarControlsWidth(ToolBarVisibility);
  FToolBarLayersControlsWidth := getToolbarControlsWidth(ToolBarLayers);
  FToolBarPointsControlsWidth := getToolbarControlsWidth(ToolBarPoints);
  FToolBarEditControlsWidth := getToolbarControlsWidth(ToolBarEdit);
  FToolBarEdgesControlsWidth := getToolbarControlsWidth(ToolBarEdges);
  FToolBarFacesControlsWidth := getToolbarControlsWidth(ToolBarFaces);
  FToolBarCurvesControlsWidth := getToolbarControlsWidth(ToolBarCurves);

  FAllToolbarsControlsWidth :=
    FToolBarFileControlsWidth +
    FToolBarVisibilityControlsWidth +
    FToolBarLayersControlsWidth +
    FToolBarPointsControlsWidth +
    FToolBarEditControlsWidth +
    FToolBarEdgesControlsWidth +
    FToolBarFacesControlsWidth +
    FToolBarCurvesControlsWidth;

  result := FAllToolbarsControlsWidth;
end;

procedure TMainForm.AlignAllToolbars;
var AllW, lft, gap:integer;
    // max bottom Y of all controls in toolbar
    function getToolbarControlsBottom(tb:TToolBar): integer;
    var i:integer; c: TControl;
    begin
      result := 0;
      for i:=0 to tb.ControlCount-1 do
        begin
          c := tb.Controls[i];
          result := max(result, c.BoundsRect.Bottom + c.BorderSpacing.Bottom+c.BorderSpacing.Around);
        end;
    end;

    // max right X of all controls in toolbar
    function getToolbarControlsRight(tb:TToolBar): integer;
    var i:integer; c: TControl;
    begin
      result := 0;
      for i:=0 to tb.ControlCount-1 do
        begin
          c := tb.Controls[i];
          result := max(result, c.BoundsRect.Right + c.BorderSpacing.Right+c.BorderSpacing.Around);
        end;
    end;

    // max right of a control in toolbar smaller than X
    function getToolbarXtrunc(tb:TToolBar; X:integer): integer;
    var i,r:integer; c: TControl;
    begin
      result := 0; r:=0;
      i:=0;
      while (i<=tb.ControlCount-1) do
        begin
          c := tb.Controls[i];
          r := r + c.Width + c.BorderSpacing.Right+c.BorderSpacing.Around;
          if (r<=X)
            then result := max(result,r);
          inc(i);
        end;
    end;

    function alignToolbar(tb:TToolBar; tbcw, lft, wdth, cnt: integer): integer;
    var tbch, r,w,bw :integer; c: TControl; b:TToolButton;
    begin
      tb.Top:=0;
      tb.Parent.Left:=lft;
      //tbcw := getToolbarControlsWidth(tb);
      //if tb.Name='ToolBarFile'
      //   then c:=c;
      bw := tb.ButtonWidth;
      if tb.ButtonCount > 0
        then
          begin
          b := tb.Buttons[0];
          bw := b.Width;
          end;

      // leave some space (bw+4)*cnt
      w:=round((wdth) * ((tbcw - gap) / (FAllToolbarsControlsWidth)));

      r:=getToolbarXtrunc(tb,w);
      if (lft+r)>(wdth-(bw+4)*cnt)
        then r:=getToolbarXtrunc(tb,r-bw);

      tb.Width := max(r+4, bw+4);
      //tb.Width := w;
      //MainForm.Caption := 'ToolBarMain.Width:' + IntToStr(ToolBarMain.Width);

      //tb.Height := 16;
      tb.ReAlign;
      tb.Invalidate;
      //tb.Width := getToolbarControlsRight(tb)+4;
      //tb.ReAlign;
      Application.ProcessMessages;
      tbch:=getToolbarControlsBottom(tb);
      tb.Height := tbch + 4;
      if PanelMain.Height < tb.Height+2 then PanelMain.Height := tb.Height + 2;
      //tb.Parent.Height := tb.Height;
      //tb.Parent.Width := tb.Width;
      //tb.InvalidatePreferredSize;
      //Application.ProcessMessages;
      result:=tb.Parent.BoundsRect.Right;
    end;
var pnlWidth: integer;
begin
  gap:=4;
  if FAllToolbarsControlsWidth=0
   then FAllToolbarsControlsWidth:=getAllToolbarsControlWidth;
  lft:=gap;
  PanelMain.Height := 20;
  pnlWidth := PanelMain.Width;
  lft := alignToolbar(ToolBarFile, FToolBarFileControlsWidth, lft+gap, pnlWidth, 7);
  lft := alignToolbar(ToolBarEdit, FToolBarEditControlsWidth, lft+gap, pnlWidth, 6);
  lft := alignToolbar(ToolBarVisibility, FToolBarVisibilityControlsWidth, lft+gap, pnlWidth, 5);
  lft := alignToolbar(ToolBarLayers, FToolBarLayersControlsWidth, lft+gap, pnlWidth, 4);
  lft := alignToolbar(ToolBarPoints, FToolBarPointsControlsWidth, lft+gap, pnlWidth, 3);
  lft := alignToolbar(ToolBarEdges, FToolBarEdgesControlsWidth, lft+gap, pnlWidth, 2);
  lft := alignToolbar(ToolBarFaces, FToolBarFacesControlsWidth, lft+gap, pnlWidth, 1);
  lft := alignToolbar(ToolBarCurves, FToolBarCurvesControlsWidth, lft+gap, pnlWidth, 0);
end;

procedure TMainForm.InitiallyLoadModel;
var FileExt: string;
begin

if FFileName = '' then
  LoadMostRecentFile;

if (FFileName = '') and not FreeShip.ModelIsLoaded then  //default behaviour if no recent file defined
  NewModelExecute(Self)
else
if (FFileName <> '') and not FreeShip.ModelIsLoaded and not FreeShip.IsLoadError then
begin
   // Skip translation
   FileExt := Uppercase(ExtractFileExt(FFileName));
   if (FileExists(FFileName) { *Converted from FileExists* })
     and ( (FileExt = '.FBM') or (FileExt = '.FTM') ) then
     begin
        FOpenHullWindows;
        FreeShip.Edit.File_Load(FFileName);
        if not FreeShip.IsLoadError then
        begin
          FreeShip.Surface.Rebuild;
          Application.ProcessMessages;
          FreeShip.ZoomFitAllViewports;
        end;
        //FreeShip.Draw;
     end
   else
   begin
      //MessageDlg(rs_Unable_to_open_file_ {UserString[106]}+' '+FFileName,mtError,[mbOk],0);
      FreeEmptyModelChooserDialog:=TFreeEmptyModelChooserDialog.Create(Self);
      //ShowTranslatedValues(FreeEmptyModelChooserDialog);
      if FreeEmptyModelChooserDialog.Execute(FFileName)
      then
        begin
        if FreeEmptyModelChooserDialog.RbCreateNew.Checked
          then NewModelExecute(Self)
        else
        if FreeEmptyModelChooserDialog.RbLoadFile.Checked
          then LoadFileExecute(Self)
        end;
      FreeEmptyModelChooserDialog.Free;
   end
   // End Skip translation
end;
SetCaption;
LoadToolIcons;
UpdateMenu;
end;

var inActivation: boolean = false;

procedure TMainForm.FormActivate(Sender: TObject);
var i:integer; splashResult:TModalResult;
begin
  if FDestroying then exit;
  if inActivation then exit;
  inActivation:=true;

  //Freeship.Edit.ProgressBar := ProgressBarMain;
  Freeship.Edit.ProgressBar := nil; // temporary turn off until empty VP resolved in Win10

  Freeship.Surface.OnFaceRebuilt:=Freeship.Edit.OnFaceRebuilt;

  // Activation is notification, not a request to change foreground order.
  // Independent windows (including the lines plan) keep their selected focus.

  if GShowSplash then
  begin
    splashResult:=ShowSplashWindow;
    if splashResult <> mrOk then
      begin
      GShowSplash:=false;
      self.Close;
      exit;
      end;
    GShowSplash:=false; // show splash on first activate only
  end;

  if not FModelInitallyLoaded then
    begin
       InitiallyLoadModel;
       FModelInitallyLoaded := true;
    end;
  Freeship.Draw;
  inActivation:=false;
end;

procedure TMainForm.AddPointToGroupExecute(Sender: TObject);
var pgf:TFreePointGroupForm;
begin
  pgf:=TFreePointGroupForm.Create(Self);
  pgf.FreeShip:=FreeShip;
  pgf.LoadGroups;
  if pgf.ShowModal = mrOk then
    begin
    end;
end;

procedure TMainForm.cbPrecisionChange(Sender: TObject);
begin
  if cbPrecision.ItemIndex = ord(FreeShip.Precision) then exit;
  FreeShip.Precision := TFreePrecisionType(cbPrecision.ItemIndex);
  FreeShip.RebuildModel;
  UpdateMenu;
end;

procedure TMainForm.AddFlowLineExecute(Sender: TObject);
begin
  FreeShip.EditMode := emAddFlowLine;
  UpdateMenu;
end;

procedure TMainForm.AddGridPanelExecute(Sender: TObject);
begin
  Freeship.Edit.Geometry_AddGridPanel;
  UpdateMenu;
end;

procedure TMainForm.ColorButton1Click(Sender: TObject);
begin
  ActiveLayerColorExecute(self);
end;

procedure TMainForm.OnSelectItem(Sender:TObject);
var Face1 : TFreeSubdivisionControlFace;
    Face2 : TFreeSubdivisionControlFace;
    Diff  : Boolean;
    I     : Integer;

begin
{
  if (Sender is TFreeSubdivisionControlPoint)
     and (Sender=FreeShip.ActiveControlPoint)
     and (not FreeShip.ActiveControlPoint.Selected)
   then
     begin
        // The active controlpoint was deselected, probably internally by the subdivision surface.
        // Set the FreeShip.ActiveControlPoint to nil (which also closes the controlpoint window)
        FreeShip.ActiveControlPoint:=nil;
     end;
}

   if FreeShip.NumberOfSelectedControlFaces>0 then
   begin
      // set the layerbox itemindex to the index of the layer of the selected controlfaces
      Face1:=FreeShip.SelectedControlFace[0];
      // check if all selected controlfaces belong to the same layer
      Diff:=False;
      for I:=1 to FreeShip.NumberOfSelectedControlFaces do
      begin
         Face2:=FreeShip.SelectedControlFace[I-1];
         if Face1.Layer<>Face2.Layer then
         begin
            Diff:=True;
            Break;
         end;
      end;
      if not Diff then
      begin
         FreeShipChangeActiveLayer(self,Face1.Layer);
      end else FreeShipChangeActiveLayer(self,nil);
   end else FreeShipChangeActiveLayer(self,FreeShip.ActiveLayer);
   UpdateMenu;
end;{TMainForm.FOnselectItem}

procedure TMainForm.OnChangeActiveControlPoint(Sender: TObject);
begin
   if (Sender = nil)
      and assigned(FreeShip)
      and assigned(FreeShip.ControlpointForm) then
     FreeShip.ControlpointForm.ActiveControlPoint := nil;
   if (Sender is TFreeSubdivisionControlPoint) then
     FreeShip.ControlpointForm.ActiveControlPoint := Sender as TFreeSubdivisionControlPoint;
   UpdateMenu;
end;

procedure TMainForm.OnChangeActiveControlEdge(Sender: TObject);
begin
  UpdateMenu;
end;

procedure TMainForm.OnChangeActiveControlFace(Sender: TObject);
begin
  UpdateMenu;
end;

procedure TMainForm.OnChangeActiveControlCurve(Sender: TObject);
begin
  UpdateMenu;
end;

procedure TMainForm.AbandonMDIChildren(AIndex: Integer);
begin
  FMDIPanelManager.Delete(AIndex);
end;

function TMainForm.ActiveMDIChild: TFreeHullWindow;
begin
  Result := FMDIPanelManager.FindActivePanel as TFreeHullWindow;
end;

function TMainForm.MDIChildCount: Integer;
begin
  Result := FMDIPanelManager.PanelCount;
end;

function TMainForm.GetMDIChildren(AIndex: Integer): TFreeHullWindow;
begin
  Result := nil;
  //if not (FormStyle in [fsMDIForm, fsMDIChild]) then
  //  exit;
  if AIndex > FMDIPanelManager.PanelCount - 1 then exit;
  Result := TFreeHullWindow(FMDIPanelManager.MDIPanels[AIndex]) ;
end;



procedure TMainForm.HullformWindowOnActivate(Sender:TObject);
begin
  // bring Action List here to process keys and shortcuts
  if Sender is TFreeHullWindow then
  begin
    if Assigned(FStudioViewButtons[0]) then FStudioViewButtons[0].Enabled := True;
    if Assigned(FStudioViewButtons[1]) then FStudioViewButtons[1].Enabled := True;
    if Assigned(FStudioViewButtons[2]) then FStudioViewButtons[2].Enabled := True;
  end;
  //FActionListHull := TFreeHullWindow(Sender).FreeHullForm.ActionListHull;
  //self.InsertComponent(FActionListHull);
end;

procedure TMainForm.HullformWindowOnDeactivate(Sender:TObject);
begin
  // remove Action List to free place for another MDI action list
  ///if not assigned(FActionListHull) then exit;
  ///if not assigned(Sender) then exit;
  ///if not assigned(TFreeHullWindow(Sender).FreeHullForm) then exit;
  ///if not assigned(TFreeHullWindow(Sender).FreeHullForm.ActionListHull) then exit;
  ///if FActionListHull <> TFreeHullWindow(Sender).FreeHullForm.ActionListHull
     ///then exit;
  ///if not FDestroying then
  ///   RemoveComponent(FActionListHull);
  ///FActionListHull:=nil;
end;

procedure TMainForm.HullformWindowOnClose(Sender:TObject; var CloseAction: TCloseAction);
begin
  HullformWindowOnDeactivate(Sender);
  FMDIPanelManager.Remove(Sender as TFreeHullWindow);
end;

procedure TMainForm.CloseHullWindows;
var thw: TFreeHullWindow;
begin
   if assigned(FMDIPanelManager) then
    while FMDIPanelManager.PanelCount>0 do
      begin
         thw:=TFreeHullWindow(FMDIPanelManager.MDIPanels[0]);
         ///if assigned(thw) and assigned(thw.FreeHullForm)
         ///  and assigned(thw.FreeHullForm.ActionListHull)
         ///  then self.RemoveComponent(thw.FreeHullForm.ActionListHull);
         if assigned(thw) then thw.Close;
       FMDIPanelManager.Remove(thw);
       end;
end;

// Creates 4 different views on the hullform
procedure TMainForm.FOpenHullWindows;
var HullformWindow: TFreeHullWindow;
    I             : Integer;
    HullformActionList:TActionList;CaptionIndex:Integer;
begin
{  StatusPanel3.Destroy;
  StatusPanel3 := TPanel.Create(Self);
  StatusPanel3.Parent := StatusBar;
  StatusPanel3.Color := clWhite;
  StatusPanel3.Align := alLeft;
  LabelDistance.Parent := StatusPanel3;
  LabelDistance.Caption := 'Distance:';

  StatusPanel4.Destroy;
  StatusPanel4 := TPanel.Create(Self);
  StatusPanel4.Parent := StatusBar;
  StatusPanel4.Color := clWhite;
  StatusPanel4.Align := alLeft;
  ProgressBarMain.Parent := StatusPanel4;
}
   if MDIChildCount=0 then
   begin
      MainClientPanel.DoubleBuffered:=False;
      MainClientPanel.DisableAlign;
      try
      for I:=0 to 3 do
      begin
         // open a new window
         HullformWindow:=TFreeHullWindow.Create(Self);
         HullformWindow.Visible:=False;
         HullformWindow.CaptionButtons:=[];
         HullformWindow.OnShow:=StudioWindowVisibility;HullformWindow.OnHide:=StudioWindowVisibility;
         HullformWindow.OnDblClick:=StudioWindowExpand;
         HullformWindow.CaptionPanel.OnDblClick:=StudioWindowExpand;
         for CaptionIndex:=0 to HullformWindow.CaptionPanel.ControlCount-1 do
           if HullformWindow.CaptionPanel.Controls[CaptionIndex] is TLabel then
             TLabel(HullformWindow.CaptionPanel.Controls[CaptionIndex]).OnDblClick:=StudioWindowExpand;
         HullformWindow.Name:='HullformWindow'+IntToStr(I);    // helps in debugging
         HullformWindow.Viewport.Name :='Viewport'+IntToStr(I);

         FMDIPanelManager.Add(HullformWindow);
         HullformWindow.Parent:=MainClientPanel;
         HullformWindow.OnMoveResize:=StudioWindowMotion;
         HullformWindow.OnActivate:=HullformWindowOnActivate;
         HullformWindow.OnDeactivate:=HullformWindowOnDeactivate;
         HullformWindow.OnClose:=HullformWindowOnClose;
         HullformWindow.OnDestroy:=HullformWindowOnDeactivate;

         //HullformWindow.Viewport.ViewType:=TFreeViewType(I);
         //ShowTranslatedValues(HullformWindow);

         HullformWindow.SetCaption;
         HullformWindow.SetBounds((I mod 2)*(MainClientPanel.ClientWidth div 2),
            (I div 2)*(MainClientPanel.ClientHeight div 2),
            MainClientPanel.ClientWidth div 2,MainClientPanel.ClientHeight div 2);
         //HullformWindow.Visible:=true;
         HullformWindow.Viewport.ViewType:=TFreeViewType(I);
         HullformWindow.FreeShip:=FreeShip;
         HullformWindow.Viewport.BeginProgressiveDisplay;
      end;
      {$ifndef LCL}
        TileMode := tbHorizontal;
      {$endif}
      for I:=0 to FMDIPanelManager.PanelCount-1 do
        TFreeHullWindow(FMDIPanelManager.MDIPanels[I]).Visible:=True;
      finally MainClientPanel.EnableAlign end;
      FStudioClientWidth:=MainClientPanel.ClientWidth;
      FStudioClientHeight:=MainClientPanel.ClientHeight;
   end;
   SetCaption;
   UpdateStudioViewButtons;
end;{TMainForm.FOpenHullWindows}

procedure TMainForm.SetCaption;
begin
   // Skip translation
   if FreeShip.FileChanged then Caption:='FreeShip_26  : '+FreeShip.Filename+' ('+rs_modified {UserString[280]}+')'
                           else Caption:='FreeShip_26  : '+FreeShip.Filename+' ('+rs_not_modified {UserString[281]}+')';
   // End Skip translation
end;{TMainForm.SetCaption}

procedure TMainForm.SetAllActionsEnabled(val : boolean);
var i: integer; A: TAction;
begin
  for i:=0 to ActionList1.ActionCount-1 do
  begin
    A := TAction(ActionList1.Actions[i]);
    A.Enabled:=val;
  end;
end;

procedure TMainForm.UpdateMenu;
var I       : Integer;
    NLayers : Integer;
    FExecDirectory     : string;
//    FileToFind         : string;
// In this procedure all actions are set to enabled/disabled according to the current state
// and selected items
  const DS=DirectorySeparator;
begin
   NLayers:=0;
   For I:=1 to Freeship.NumberOfLayers do if Freeship.Layer[I-1].Count>0 then inc(NLayers);

   {
   // disable almost all actions if model was not loaded for some reason
   if not Freeship.ModelLoaded then
     begin
        FreeShip.Filename:='MODEL NOT LOADED!';
        FreeShip.FilenameSet:=false;

        SetAllActionsEnabled(false);

        NewModel.Enabled:=true;
        LoadFile.Enabled:=true;
        ExitProgram.Enabled:=true;
        AboutAction.Enabled:=true;

        //ImportFEF.Enabled:=true;
        //ImportCarene.Enabled:=true;
        //ImportVRML.Enabled:=true;
        //ImportHullFile.Enabled:=true;
     end;
    }

   // File menu
   FileSaveas.Enabled := (FreeShip.Surface.NumberOfControlPoints>0)
       or (Freeship.FileChanged) or (Freeship.FilenameSet);
   FileSave.Enabled:= FileSaveas.Enabled and Freeship.FileChanged
     and not Freeship.FileIsReadOnly;

   ImportMichletWaves1.Enabled:=(MDIChildCount>0) and (Freeship.Surface.NumberOfControlFaces>1);
   ExportFEF.Enabled:=Freeship.Surface.NumberOfControlPoints>0;
   ExportObj.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   ExportSTL.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   ExportDXF3DPolylines.Enabled:=((FreeShip.NumberofStations>0) and (FreeShip.Visibility.ShowStations)) or
                                 ((Freeship.NumberofButtocks>0) and (Freeship.Visibility.ShowButtocks)) or
                                 ((Freeship.NumberofWaterlines>0) and (FreeShip.Visibility.ShowWaterlines)) or
                                 ((FreeShip.NumberofDiagonals>0) and (FreeShip.Visibility.ShowDiagonals)) or
                                 ((FreeShip.NumberofControlcurves>0) and (FreeShip.Visibility.ShowControlcurves));
   ExportDXF2DPolylines.Enabled:=((FreeShip.NumberofStations>0) and (FreeShip.Visibility.ShowStations)) or
                                 ((Freeship.NumberofButtocks>0) and (Freeship.Visibility.ShowButtocks)) or
                                 ((Freeship.NumberofWaterlines>0) and (FreeShip.Visibility.ShowWaterlines));
   ExportDXFFaces.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   ExportIGES.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   ExportOffsets.Enabled:=FreeShip.NumberofStations+Freeship.NumberofButtocks+Freeship.NumberofWaterlines+
                          Freeship.NumberofDiagonals+Freeship.NumberOfControlCurves>0;
   ExportArchimedes.Enabled:=FreeShip.NumberofStations>0;
   ExportGHS.Enabled:=FreeShip.NumberofStations>0;
   ExportPAM.Enabled:=FreeShip.NumberofStations>0;   
// Detect existance of external modules
   FExecDirectory:=Freeship.Preferences.ExecDirectory;
   ExportMichlet.Enabled:=(Freeship.Surface.NumberOfControlFaces>0) and (Freeship.ProjectSettings.MainparticularsHasBeenset);
   ExportAddMass.Enabled:=(Freeship.Surface.NumberOfControlFaces>0) and (Freeship.ProjectSettings.MainparticularsHasBeenset);   
   HydrodynTask1_.Enabled:=False;
   HydrodynTask2_.Enabled:=False;
   HydrodynTask3_.Enabled:=False;
   HydrodynTask4_.Enabled:=False;
   PropTask1_.Enabled:=False;
   PropTask2_.Enabled:=False;
   PropTask3_.Enabled:=False;
   PropTask4_.Enabled:=False;
   PropTask5_.Enabled:=False;
   ResistanceHollen.Enabled:=False;
   ResistanceOST.Enabled:=False;
   ResistanceOortmer.Enabled:=False;
   ResistanceFungLeib.Enabled:=False;
   HydrodynManeuv_.Enabled:=False;
   PropTaskRvrs_.Enabled:=False;
   ExportAddMass.Enabled:=False;
   ResistanceRBHS.Enabled:=False;
   ResistanceMH.Enabled:=False;   
   // Detect existance of external modules and include executables into menu
   if FileExists(FExecDirectory+DS+'ADD_MASS.EXE') { *Converted from FileExists* } then ExportAddMass.Enabled:=True;
   if FileExists(FExecDirectory+DS+'Ishercof.EXE') { *Converted from FileExists* } then HydrodynTask1_.Enabled:=True;
   if FileExists(FExecDirectory+DS+'Pos_Ship.EXE') { *Converted from FileExists* } then HydrodynTask2_.Enabled:=True;
   if FileExists(FExecDirectory+DS+'Rot_Ship.EXE') { *Converted from FileExists* } then HydrodynTask3_.Enabled:=True;
   if FileExists(FExecDirectory+DS+'HOLLENBH.EXE') { *Converted from FileExists* } then ResistanceHollen.Enabled:=True;
   if FileExists(FExecDirectory+DS+'OORTMERS.EXE') { *Converted from FileExists* } then ResistanceOortmer.Enabled:=True;
   if FileExists(FExecDirectory+DS+'fungleib.EXE') { *Converted from FileExists* } then ResistanceFungLeib.Enabled:=True;
   if FileExists(FExecDirectory+DS+'hship.EXE') { *Converted from FileExists* }    then ResistanceOST.Enabled:=True;
   if FileExists(FExecDirectory+DS+'hship.EXE') { *Converted from FileExists* }    then ResistanceRBHS.Enabled:=True;
   if FileExists(FExecDirectory+DS+'hship.EXE') { *Converted from FileExists* }    then ResistanceMH.Enabled:=True;
   if FileExists(FExecDirectory+DS+'ManeuvPP.EXE') { *Converted from FileExists* } then HydrodynManeuv_.Enabled:=True;
   if FileExists(FExecDirectory+DS+'CALCPROP.EXE') { *Converted from FileExists* } then begin
      PropTask1_.Enabled:=True;
      PropTask2_.Enabled:=True;
      PropTask3_.Enabled:=True;
   end;
   if FileExists(FExecDirectory+DS+'PropPred.EXE') { *Converted from FileExists* } then PropTask4_.Enabled:=True;
   if FileExists(FExecDirectory+DS+'PROPOL.EXE') { *Converted from FileExists* }   then PropTask5_.Enabled:=True;
   if FileExists(FExecDirectory+DS+'RVRSSHIP.EXE') { *Converted from FileExists* } then PropTaskRvrs_.Enabled:=True;
   if ExportAddMass.Enabled then ExportAddMass.Enabled:=(Freeship.Surface.NumberOfControlFaces>0) and (Freeship.ProjectSettings.MainparticularsHasBeenset);   
//   if HydrodynTask2_.Enabled then HydrodynTask2_.Enabled:=(Freeship.Surface.NumberOfControlFaces>0) and (Freeship.ProjectSettings.MainparticularsHasBeenset);   
//   HydrodynTask3_.Enabled:=False; //(Freeship.Surface.NumberOfControlFaces>0) and (Freeship.ProjectSettings.MainparticularsHasBeenset);   
//   HydrodynTask4_.Enabled:=False; //(Freeship.Surface.NumberOfControlFaces>0) and (Freeship.ProjectSettings.MainparticularsHasBeenset);   
   RecentFiles.Enabled:=RecentFiles.Count>0;
   ExportCoordinates.Enabled:=Freeship.Surface.NumberOfControlPoints>0;
   ExportPart.Enabled:=(Freeship.Surface.NumberOfControlFaces>0);
   ImportPart.Enabled:=(Freeship.Surface.NumberOfControlFaces>0) and (MDIChildCount>0);
   // Show controledges and controlpoints
   ShowControlNet.Enabled:=FreeShip.Surface.NumberOfControlPoints>0;
   ShowControlNet.Checked:=FreeShip.Visibility.ShowControlNet;

   // Show free (not having faces) points and edges
   ShowFreeObjects.Enabled:=(FreeShip.Surface.NumberOfControlPoints>0);
   ShowFreeObjects.Checked:=FreeShip.Visibility.ShowFreeObjects;

   // Show interior edges
   ShowInteriorEdges.Enabled:=FreeShip.Surface.NumberOfControlFaces
                             +FreeShip.NumberofControlcurves > 0;
   ShowInteriorEdges.Checked:=FreeShip.Visibility.ShowInteriorEdges;
   // Show both sides
   BothSides.Checked:=Freeship.Visibility.ModelView=mvBoth;
   BothSides.Enabled:=FreeShip.Surface.NumberOfControlFaces>0;
   // Delete
   Delete.Enabled := Freeship.NumberOfSelectedControlPoints
                    +FreeShip.NumberOfSelectedControlEdges
                    +Freeship.NumberOfSelectedControlFaces
                    +Freeship.NumberOfSelectedControlCurves
                    +Freeship.NumberOfSelectedFlowLines
                    +Freeship.NumberOfselectedMarkers > 0;
   // Window menu actions
   TileWindow.Enabled:=MDIChildCount>0;
   CascadeWindow.Enabled:=MDIChildCount>0;
   // Precision
   //PrecisionBox.ItemIndex:=Ord(FreeShip.Precision);
   SpinEditFontSize.Value:=FreeShip.Preferences.FontSize;
   // Layers
   LayerAutoGroup.Enabled:=(Freeship.Surface.NumberOfControlFaces>1) and (FreeShip.Visibility.ShowInteriorEdges);
   // Tools
   CheckModel.Enabled:=FreeShip.Surface.NumberOfControlFaces>0;
   DevelopLayers.Enabled:=False;
   for I:=1 to FreeShip.NumberOfLayers do
     if (FreeShip.Layer[I-1].Developable) and (FreeShip.Layer[I-1].Count>0)
     then
       begin
          DevelopLayers.Enabled:=True;
          break;
       end;
   KeelRudderWizard.Enabled:=MDIChildCount>0;
   DeleteMarkers.Enabled:=Freeship.NumberofMarkers>0;
   // Calculations
   DesignHydrostatics.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   Hydrostaticsdialog.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   CrossCurves.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   // edit commands
   AddPoint.Enabled:=(MDIChildCount>0) and (FreeShip.Visibility.ShowControlNet);
   UpdateStudioModeButtons;
   Insertplane.Enabled:=(Freeship.Surface.NumberOfControlEdges>0) and (Freeship.Visibility.ShowControlNet);
   LayerIntersection.Enabled:=NLayers>1;
   EdgeCollapse.Enabled:=FreeShip.NumberOfSelectedControlEdges>0;
   NewEdge.Enabled:= (FreeShip.NumberOfSelectedControlPoints>1) and FreeShip.Surface.CanInsertEdge;
   EdgeCrease.Enabled:=FreeShip.NumberOfSelectedControlEdges>0;
   DeselectAll.Enabled:=(Freeship.NumberOfSelectedControlPoints+FreeShip.NumberOfSelectedControlEdges+Freeship.NumberOfSelectedControlFaces+Freeship.NumberOfSelectedControlCurves>0) or
                        (Freeship.ActiveControlPoint<>nil);
   NewCurve.Enabled:=FreeShip.NumberOfSelectedControlEdges>0;
   PointCollapse.Enabled:=Freeship.NumberOfSelectedControlPoints>0;
   DeleteEmptyLayers.Enabled:=False;
   for I:=1 to Freeship.NumberOfLayers do
     if (FreeShip.ModelIsLoaded) and (FreeShip.Layer[I-1].Count=0) and (FreeShip.NumberOfLayers>0) then
       begin
          DeleteEmptyLayers.Enabled:=True;
          break;
       end;
   RemoveUnusedPoints.Enabled:=False;
   for I:=1 to Freeship.Surface.NumberOfControlPoints do if Freeship.Surface.ControlPoint[I-1].NumberOfFaces=0 then
   begin
      RemoveUnusedPoints.Enabled:=True;
      break;
   end;
   InvertFace.Enabled:=Freeship.NumberOfSelectedControlFaces>0;
   ShowStations.Enabled:=Freeship.NumberofStations>0;
   ShowStations.Checked:=FreeShip.Visibility.ShowStations;
   ShowButtocks.Enabled:=Freeship.NumberofButtocks>0;
   ShowButtocks.Checked:=FreeShip.Visibility.ShowButtocks;
   ShowWaterlines.Enabled:=Freeship.NumberofWaterlines>0;
   ShowWaterlines.Checked:=FreeShip.Visibility.ShowWaterlines;
   ShowDiagonals.Enabled:=Freeship.NumberofDiagonals>0;
   ShowDiagonals.Checked:=FreeShip.Visibility.ShowDiagonals;
   ShowNormals.Checked:=FreeShip.Visibility.ShowNormals;
   ShowNormals.Enabled:=FreeShip.NumberOfSelectedControlFaces>0;
   ShowGrid.Checked:=Freeship.Visibility.ShowGrid;
   ShowGrid.Enabled:=Freeship.NumberofStations+Freeship.NumberofButtocks+Freeship.NumberofWaterlines+Freeship.NumberofDiagonals>0;
   ShowMarkers.Checked:=FreeShip.Visibility.ShowMarkers;
   ShowMarkers.Enabled:=Freeship.NumberofMarkers>0;
   ShowCurvature.Checked:=FreeShip.Visibility.ShowCurvature;
   ShowCurvature.Enabled:=Freeship.NumberofStations+Freeship.NumberofButtocks+Freeship.NumberofWaterlines+Freeship.NumberofDiagonals+Freeship.NumberOfControlCurves>0;
   ShowControlCurves.Checked:=FreeShip.Visibility.ShowControlCurves;
   ShowControlCurves.Enabled:=Freeship.Surface.NumberOfControlCurves>0;
   ShowHydrostatics.Checked:=Freeship.Visibility.ShowHydrostaticData;
   ShowHydrostatics.Enabled:=(Freeship.Surface.NumberOfControlFaces>2) and (Freeship.ProjectSettings.MainparticularsHasBeenset);
   ShowFlowlines.Checked:=Freeship.Visibility.ShowFlowlines;
   ShowFlowlines.Enabled:=Freeship.NumberOfFlowLines>0; 

   NewFace.Enabled:=FreeShip.NumberOfSelectedControlPoints>2;
   IntersectionDialog.Enabled:=FreeShip.Surface.NumberOfControlFaces>0;
   EdgeExtrude.Enabled:=FreeShip.NumberOfSelectedControlEdges>0;
   EdgeSplit.Enabled:=FreeShip.NumberOfSelectedControlEdges>0;
   RotateModel.Enabled:=FreeShip.Surface.NumberOfControlPoints>0;
   RotateModelM.Enabled:=FreeShip.Surface.NumberOfControlPoints>0;
   ScaleModel.Enabled:=FreeShip.Surface.NumberOfControlPoints>0;
   MoveModel.Enabled:=FreeShip.Surface.NumberOfControlPoints>0;
   Mirrorface.Enabled:=Freeship.Surface.NumberOfControlFaces>0;
   // Skip translation
   if (Freeship.Undoposition-1>=0) and (Freeship.Undoposition-1<Freeship.UndoCount) then
   begin
      Undo.Caption:=rs_Undo {UserString[290]}+#32+Freeship.UndoObject[Freeship.Undoposition-1].Undotext;
   end else
   begin
      Undo.Caption:=rs_Undo {UserString[290]};
   end;
   Undo.Enabled:=(FreeShip.UndoCount>0) and (Freeship.UndoPosition>0);
   //if Undo.Enabled then Undo.Caption:='Undo '+Freeship.UndoObject[Freeship.Undoposition-1].Undotext
   //                else Undo.Caption:='Undo';
   Redo.Enabled:=(FreeShip.UndoCount>0) and (Freeship.UndoPosition<Freeship.UndoCount);
//   if Redo.Enabled then Redo.Caption:='Redo '+Freeship.UndoObject[Freeship.Undoposition].Undotext
//                   else Undo.Caption:='Redo';
   // End Skip translation
   Undohistory1.Enabled:=Freeship.UndoCount>0;
   ClearUndo.Enabled:=Freeship.UndoCount>0;
   PointsLock.Enabled:=(Freeship.NumberOfSelectedControlPoints>0) and (Freeship.NumberOfSelectedLockedPoints<Freeship.NumberOfSelectedControlPoints);
   PointsUnlock.Enabled:=Freeship.NumberOfSelectedLockedPoints>0;
   PointsUnlockAll.Enabled:=Freeship.NumberOfLockedPoints>0;
   PointAlign.Enabled:=Freeship.NumberOfSelectedControlPoints>2;
   PointAnchor.Enabled:=Freeship.NumberOfSelectedControlPoints>1;
   PointsCoincide.Enabled:=Freeship.NumberOfSelectedControlPoints>1;
   PointExtrude.Enabled:=Freeship.NumberOfSelectedControlPoints>0;
   TransformLackenby.Enabled:=Freeship.Surface.NumberOfControlFaces>0;

   if cbPrecision.ItemIndex <> ord(FreeShip.Precision) then
      cbPrecision.ItemIndex := ord(FreeShip.Precision);

   FreeShip.ControlpointForm.Reload;
   FreeShip.ControlpointForm.FormStyle:=fsSystemStayOnTop;
   UpdateStudioViewButtons;
end;{TMainForm.UpdateMenu}


procedure TMainForm.RecentFilesDialogActivate(Sender: TObject);
var dlg: TTileDialog; i:integer; vFileName,sTime:string; jpg:TJPEGImage;
    pic:TPicture;
begin
  dlg:= Sender as TTileDialog;
  dlg.Cursor := crHourGlass;
  Screen.Cursor := crHourGlass;
  Application.ProcessMessages;
  for i:=0 to FreeShip.Edit.RecentFileCount-1 do
  begin
    vFileName:=Freeship.Edit.RecentFile[i];
    if not FileExists(vFileName) then continue;
    sTime:=FormatDateTime('YYYY-MM-DD hh:mm:ss',FileDateToDateTime(FileAgeUTF8(vFileName)));
    Screen.Cursor := crHourGlass;
    dlg.Cursor := crHourGlass;
    Application.ProcessMessages;

    Freeship.Edit.ProgressBar := dlg.ProgressBar1;
    jpg := nil;
    try
      jpg := Freeship.Edit.getPreviewImage(vFileName);
    except
      on E:Exception do
      begin
        Logger.Error(E.Message);
        Logger.Error(Logger.GetExceptionCallStack(E));
        if assigned(jpg) then FreeAndNil(jpg);
      end;
    end;
    if assigned(jpg) then
      begin
      pic := TPicture.Create;
      pic.Bitmap.Assign(jpg);
      dlg.AddTile(pic, sTime+' - '+vFileName, vFileName);
      end
    else
    begin
      pic := TPicture.Create;
      pic.Bitmap.SetSize(100,100);
      pic.Bitmap.Canvas.Brush.Color:=clYellow;
      pic.Bitmap.Canvas.FillRect(0,0,100,100);
      pic.Bitmap.Canvas.Font.Color:=clRed;
      pic.Bitmap.Canvas.TextOut(5,40, 'Load Error');
      dlg.AddTile(pic, sTime+' - '+vFileName, vFileName);
    end;
    Application.ProcessMessages;
  end;
  Screen.Cursor := crDefault;
  dlg.Cursor := crDefault;
end;

procedure TMainForm.ShowRecentFilesDialog;
var dlg: TTileDialog; i:integer; vFileName,sTime:string; jpg:TJPEGImage;
    pic:TPicture;
begin
  dlg := TTileDialog.create(Self);
  MenuImages.GetBitmap(ToolButtonOpenFile.ImageIndex, dlg.ButtonOpenFile.Glyph);

  dlg.FileList := Freeship.Edit.RecentFiles;
  dlg.onActivate := RecentFilesDialogActivate;
  Freeship.Edit.ProgressBar := dlg.ProgressBar1;
  Freeship.Surface.OnFaceRebuilt:=Freeship.Edit.OnFaceRebuilt;

  dlg.ShowModal;
  FreeShipUpdateRecentFileList(nil); //update just in case if items deleted

  vFileName:=dlg.FileName;
  dlg.Free;

  Freeship.Surface.OnFaceRebuilt:=Freeship.Edit.OnFaceRebuilt;
  Freeship.Edit.ProgressBar := self.ProgressBarMain;

  if (vFileName<>'') and (vFileName<>'*') then
    begin
    if not FreeShip.Edit.CheckIfChangedAndAskToSave then Exit;
    FreeShip.Edit.File_Load(vFileName);
    if FreeShip.IsLoadError then
    begin
      Freeship.Edit.ProgressBar := nil;
      Exit;
    end;
    FreeShip.Surface.Rebuild;
    FreeShip.Draw;
    end;
  if (vFileName = '*') then
    begin
    FreeShip.Edit.File_Load;
    if FreeShip.IsLoadError then
    begin
      Freeship.Edit.ProgressBar := nil;
      Exit;
    end;
    {FreeShip.Surface.Rebuild;
    FreeShip.Draw;
    SetCaption;
    UpdateMenu;}
    end;

end;

procedure TMainForm.LoadFileExecute(Sender: TObject);
begin
  FOpenHullWindows;
  FreeShip.Edit.File_Load;
  if FreeShip.IsLoadError then
  begin
    Freeship.Edit.ProgressBar := nil;
    Exit;
  end;

  if FileExistsUTF8(FreeShip.Filename) then
     FreeShip.FileIsReadOnly := FreeShip.FileIsReadOnly or FileIsReadOnlyUTF8(FreeShip.Filename);

  Application.ProcessMessages;
  FreeShip.ZoomFitAllViewports;
  //FreeShip.Draw;
  SetCaption;
  UpdateMenu;
end;{TMainForm.LoadFileExecute}

procedure TMainForm.ExitProgramExecute(Sender: TObject);
begin
  UpdateMenu;
  if FileExists('Resist.dat')  then DeleteFile('Resist.dat');
  if FileExists('RESISTp.dat')  then DeleteFile('RESISTp.dat');
  if FileExists('Vint1.dat')  then DeleteFile('Vint1.dat');
  if FileExists('SAC.tmp')  then DeleteFile('SAC.tmp');
  Close;
end;{TMainForm.ExitProgramExecute}

procedure TMainForm.LayerBoxPanelClick(Sender: TObject);
begin

end;

procedure TMainForm.PointExtrudeExecute(Sender: TObject);
begin
  Freeship.Edit.Point_Extrude;
  UpdateMenu;
end;

procedure TMainForm.PointsCoincideExecute(Sender: TObject);
begin
  // get multiple selected points to the location of a first selected one
  Freeship.Edit.Point_CoinsideToPoint;
  UpdateMenu;
end;

procedure TMainForm.SelectionDialogExecute(Sender: TObject);
begin
  if FormSelected = nil then
    FormSelected := TFormSelected.Create(Self);
  FormSelected.FreeShip := FreeShip;
  FreeShip.Surface.AddOnSelectItemListener(FormSelected.onSelectionUpdate);
  FreeShip.Surface.AddOnChangeItemListener(FormSelected.onSelectionUpdate);
  FreeShip.Surface.AddOnChangeActiveControlPointListener(FormSelected.onSelectionUpdate);
  FreeShip.Surface.AddOnChangeActiveControlEdgeListener(FormSelected.onSelectionUpdate);
  FreeShip.Surface.AddOnChangeActiveControlFaceListener(FormSelected.onSelectionUpdate);
  FreeShip.Surface.AddOnChangeActiveControlCurveListener(FormSelected.onSelectionUpdate);
  FormSelected.onSelectionUpdate(Self);
  FormSelected.Show;
end;

procedure TMainForm.SplitSectionDialogExecute(Sender: TObject);
begin
  if FSplitSectionDialog = nil then
     FSplitSectionDialog:=TFreeSplitSectionDialog.Create(Self);
  FSplitSectionDialog.SetDimensions(FreeShip.Surface.Max.X,
                         FreeShip.Surface.Max.X/2, //TODO replace with real Widest
                         FreeShip.Surface.Max.X/2);
  if FreeShip.ProjectSettings.ProjectSplitSectionLocation > 0 then
     FSplitSectionDialog.SplitSectionLocation := FreeShip.ProjectSettings.ProjectSplitSectionLocation
  else
     FSplitSectionDialog.SplitSectionLocation := FreeShip.Surface.Max.X/2;
  FSplitSectionDialog.OnSplitSectionLocationChange:=OnSplitSectionLocationChange;
  FreeShip.ProjectSettings.UseDefaultSplitSectionLocation := false;
  FSplitSectionDialog.Show;
  FreeShip.ProjectSettings.ProjectSplitSectionLocation:=FSplitSectionDialog.SplitSectionLocation;
  //FSplitSectionDialog.Free;
  //FreeShip.FileChanged := True;
  //FreeShip.Redraw;
  //UpdateMenu;
end;

procedure TMainForm.OnSplitSectionLocationChange(Sender: TObject; aValue: TFloatType);
begin
  FreeShip.ProjectSettings.ProjectSplitSectionLocation:=aValue;
  FreeShip.FileChanged := True;
  FreeShip.Redraw;
end;

procedure TMainForm.ShowFreeObjectsExecute(Sender: TObject);
begin
  FreeShip.Visibility.ShowFreeObjects:=not FreeShip.Visibility.ShowFreeObjects;
  UpdateMenu;
end;

procedure TMainForm.LayerVisibilityDialogExecute(Sender: TObject);
begin
  if FreeLayerVisibilityDialog = nil then
    FreeLayerVisibilityDialog:= TFreeLayerVisibilityDialog.Create(Self);
  FreeLayerVisibilityDialog.FreeShip := FreeShip;
  FreeLayerVisibilityDialog.OnChange := FreeLayerVisibilityDialogChange;
  FreeLayerVisibilityDialog.Show;
end;

procedure TMainForm.FreeLayerVisibilityDialogChange(Sender:TObject);
begin
  UpdateMenu;
end;

function TMainForm.ShowSplashWindow:TModalResult;
begin
  if GShowSplash then
  begin
   //ShowTranslatedValues(FreeSplashWindow);
   FreeSplashWindow:=TFreeSplashWindow.Create(Application);
   try
   SplashWindow := FreeSplashWindow;
   FreeSplashWindow.FreeShip:=FreeShip;
   FreeSplashWindow.Position:=poMainFormCenter;
   FreeSplashWindow.ShowModal;
   result:=FreeSplashWindow.ModalResult;
   finally
      FreeSplashWindow.Free;
      FreeSplashWindow:=nil;
      SplashWindow:=nil;
   end;
  end;
end;

procedure TMainForm.FormShow(Sender: TObject);
var FileExt: string; L,T,W,H:integer;
begin
   // Initialize some data
   FreeShip.OnChangeActiveLayer:=FreeShipChangeActiveLayer;
   Freeship.OnChangeLayerData:=FreeShipChangeLayerData;
   //FreeShip.OnSelectItem:=OnSelectItem;
   FreeShip.Surface.AddOnSelectItemListener(OnSelectItem);
   FreeShip.Surface.AddOnChangeActiveControlPointListener(OnChangeActiveControlPoint);
   FreeShip.Surface.AddOnChangeActiveControlEdgeListener(OnChangeActiveControlEdge);
   FreeShip.Surface.AddOnChangeActiveControlFaceListener(OnChangeActiveControlFace);
   FreeShip.Surface.AddOnChangeActiveControlCurveListener(OnChangeActiveControlCurve);
   FreeShip.Clear;

   // fit to screen
   L:=Left; T:=Top; W:=Width; H:=Height;
   if self.BoundsRect.Right > Screen.Width then L:=0;
   if self.Width > Screen.Width then W:=Screen.Width;
   if self.BoundsRect.Bottom > Screen.Height then T:=0;
   if self.Height > Screen.Height then H:=Screen.Height;
   self.SetBounds(L,T,W,H);

   SetCaption;
   LoadToolIcons;
   UpdateMenu;
   ArrangeRibbonPanel(PanelMain);

end;{TMainForm.FormShow}

procedure TMainForm.MainClientPanelClick(Sender: TObject);
begin

end;

procedure TMainForm.MenuItem1Click(Sender: TObject);
begin

end;

procedure TMainForm.PanelMainResize(Sender: TObject);
begin
  if PanelMain.IsResizing then exit;
  ArrangeRibbonPanel(PanelMain);
end;

procedure TMainForm.DismissPointEditorOnInput(Sender: TObject; Msg: Cardinal);
begin
  if not Assigned(FreeShip) or not Assigned(FreeShip.ControlpointForm) or
     not FreeShip.ControlpointForm.Visible then Exit;
  if (Sender is TControl) and
     (GetParentForm(TControl(Sender)) = FreeShip.ControlpointForm) then Exit;
  case Msg of
    LM_LBUTTONDOWN, LM_RBUTTONDOWN, LM_MBUTTONDOWN, LM_MOUSEWHEEL,
    LM_KEYDOWN, LM_SYSKEYDOWN: FreeShip.ControlpointForm.Hide;
  end;
end;

procedure TMainForm.ShowControlNetExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowControlNet:=not FreeShip.Visibility.ShowControlNet;
   UpdateMenu;
end;{TMainForm.ShowControlNetExecute}

procedure TMainForm.ShowInteriorEdgesExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowInteriorEdges:=not FreeShip.Visibility.ShowInteriorEdges;
   UpdateMenu;
end;{TMainForm.ShowInteriorEdgesExecute}

procedure TMainForm.NewWindowExecute(Sender: TObject);
var HullformWindow : TFreeHullWindow;
begin
   // Create loads the viewport resource; CreateNew leaves it uninitialized.
   HullformWindow:=TFreeHullWindow.Create(Self);
   HullformWindow.Visible:=False;
   HullformWindow.CaptionButtons:=[cbMaximize,cbMinimize,cbRestore];
   HullformWindow.Parent:=MainClientPanel;
   HullformWindow.OnMoveResize:=StudioWindowMotion;
   HullformWindow.OnActivate:=HullformWindowOnActivate;
   HullformWindow.OnDeactivate:=HullformWindowOnDeactivate;
   HullformWindow.OnClose:=HullformWindowOnClose;
   HullformWindow.OnDestroy:=HullformWindowOnDeactivate;
   FMDIPanelManager.Add(HullformWindow);
   HullformWindow.SetBounds(0,0,MainClientPanel.ClientWidth div 2,
      MainClientPanel.ClientHeight div 2);
   HullformWindow.FreeShip:=FreeShip;
   HullformWindow.Viewport.ViewType:=fvPerspective;
   HullformWindow.SetCaption;
   HullformWindow.Viewport.BeginProgressiveDisplay;
   HullformWindow.Visible:=True;
   UpdateMenu;
end;{TMainForm.NewWindowExecute}

procedure TMainForm.SpinEditFontSizeChange(Sender: TObject);
var w: integer;  vp:TFreeViewport;
begin
  FreeShip.Preferences.FontSize := SpinEditFontSize.value;
  if SpinEditFontSize.value < 10
  then SpinEditFontSize.Constraints.MinWidth := 16+24+2
  else SpinEditFontSize.Constraints.MinWidth := 16+16+24+2;
  SpinEditFontSize.Width := SpinEditFontSize.Constraints.MinWidth;

  for w:=0 to FMDIPanelManager.PanelCount-1 do
  begin
    vp := TFreeHullWindow(FMDIPanelManager.MDIPanels[w]).Viewport;
    vp.invalidate;
  end;
end;


procedure TMainForm.Tile;
var T,L,MW,MH, X,Y,W,H, i, MCC, r,c,rows,cols : Integer;
    HFW : TFreeHullWindow;
begin
 {$ifdef Windows}
  inherited Tile;
 {$endif}

 {$ifdef LCLQt}
  inherited Tile;
 {$else}
 // do manual tile for non MDI environments

 //HH := GetSystemMetrics(SM_CYCAPTION); //header height
 //FW := GetSystemMetrics(SM_CYDLGFRAME); //frame width

// Color:=clRed;

 MCC := MainClientPanel.ControlCount;
 rows:=round(sqrt(MCC));
 cols:=round(sqrt(MCC));

 MW := MainClientPanel.ClientWidth;
 MH := MainClientPanel.ClientHeight;
 W := MW div rows;
 H := MH div cols;
 T := 0;
 L := 0;

 //MCC := MdiChildCount;
 c:=0; r:=0;
 for i:= 0 to MCC - 1 do
   begin
   //HFW := MDIChildren[i];
   HFW := TFreeHullWindow(MainClientPanel.Controls[i]);
   if Assigned(HFW) then
     begin
     HFW.SetBounds(L+(c*W), T+(r*H), W, H);
     inc(c);
     if c >= cols then begin c:=0; inc(r); end;
     end;
   end;
 {$endif}
end;{TMainForm.Tile}

procedure TMainForm.Cascade;
var T,L,MW,MH, X,Y,W,H, i, MCC : Integer;
   Ctrl:TControl; HFW : TFreeHullWindow;
begin
  {$ifdef Windows}
   inherited Cascade;
  {$endif}

  {$ifdef LCLQt}
   inherited Cascade;
  {$else}
  // do manual cascade for non MDI environments
  MW := MainClientPanel.ClientWidth;
  MH := MainClientPanel.ClientHeight;
  T := 0;
  L := 0;
  MCC:=MainClientPanel.ControlCount;
  W := MW-40*MCC;
  H := MH-30*MCC;
  if W<200 then W:=200;
  for i:= 0 to MCC - 1 do
   begin
   //HFW := MDIChildren[i];
   Ctrl:=MainClientPanel.Controls[i];
   if not (Ctrl is TFreeHullWindow) then continue;
   HFW := TFreeHullWindow(MainClientPanel.Controls[i]);
   HFW.SetBounds(i*40,i*30, W,H);
   end;
  {$endif}
end;{TMainForm.Cascade}

procedure TMainForm.TileWindowExecute(Sender: TObject);
begin
  {$ifndef LCL}{$ifndef CLX}
  TileMode := tbHorizontal;
  {$endif}{$endif}
  //Tile;
  FMDIPanelManager.Tile;
end;{TMainForm.TileWindowExecute}

procedure TMainForm.CascadeWindowExecute(Sender: TObject);
begin
  {$ifndef LCL}{$ifndef CLX}
  TileMode := tbHorizontal;
  {$endif}{$endif}
  FMDIPanelManager.Cascade;
end;{TMainForm.CascadeWindowExecute}

procedure TMainForm.BothSidesExecute(Sender: TObject);
begin
   if FreeShip.Visibility.ModelView=mvBoth then FreeShip.Visibility.ModelView:=mvPort
                                           else FreeShip.Visibility.ModelView:=mvBoth;
   UpdateMenu;
end;{TMainForm.BothSidesExecute}

procedure TMainForm.FreeShipFileChanged(Sender: TObject);
begin
   SetCaption;
end;{TMainForm.FreeShipFileChanged}

{ //moved to TFREEProjectSettingsDialog
procedure TMainForm.PrecisionBoxChange(Sender: TObject);
begin
   FreeShip.Precision:=TFreePrecisionType(PrecisionBox.ItemIndex);
   UpdateMenu;
end;}{TMainForm.PrecisionBoxChange}


procedure TMainForm.FileSaveasExecute(Sender: TObject);
begin
   FreeShip.Edit.File_SaveAs;
   UpdateMenu;
   SetCaption;
end;{TMainForm.SaveFileExecute}

procedure TMainForm.LayerAutoGroupExecute(Sender: TObject);
begin
   FreeShip.Edit.Layer_AutoGroup;
   UpdateMenu;
end;{TMainForm.LayerAutoGroupExecute}

procedure TMainForm.FLoadRecentFile(sender:TObject);
var Menu    : TMenuItem;
    Filename: string;
    N       : Integer;
    Answer  : word;
begin
   if sender is TMenuItem then
   begin
      Menu:=sender as TMenuItem;
      // Skip translation
      Filename:=Menu.Caption;
      repeat
         N:=Pos('&',Filename);
         if N<>0 then system.Delete(Filename,N,1);
      until N=0;

   if Freeship.FileChanged then
   begin
      Answer:=MessageDlg(rs_The_current_model_has_been_changed_ {UserString[103]}+EOL+rs_Do_you_want_to_save_it_first_ {UserString[104]},mtConfirmation,[mbYes,mbNo,mbCancel],0);
      if Answer=mrCancel then exit;
      if Answer=mrYes then
      begin
         Freeship.Edit.File_SaveAs;
         if Freeship.FileChanged then exit; // Apparently saving was not successfull, abort
      end;
   end;

   // End Skip translation
   if FileExists(Filename)  then
      begin
         Freeship.Edit.File_Load(Filename);
         if FreeShip.IsLoadError then
         begin
           Freeship.Edit.ProgressBar := nil;
           Exit;
         end;
         FOpenHullWindows;
         SetCaption;
         UpdateMenu;
      end
   else
      begin
         //MessageDlg(rs_Unable_to_open_file_ {UserString[106]}+' '+FFileName,mtError,[mbOk],0);
         FreeEmptyModelChooserDialog:=TFreeEmptyModelChooserDialog.Create(Self);
         //ShowTranslatedValues(FreeEmptyModelChooserDialog);
         if FreeEmptyModelChooserDialog.Execute(FileName)
         then
           begin
           if FreeEmptyModelChooserDialog.RbCreateNew.Checked
             then NewModelExecute(Self)
           else
           if FreeEmptyModelChooserDialog.RbLoadFile.Checked
             then LoadFileExecute(Self)
           end;
         FreeEmptyModelChooserDialog.Free;
      end
   end;
end;{TMainForm.FLoadRecentFile}

procedure TMainForm.LoadMostRecentFile;
var Menu    : TMenuItem;
    Filename: string;
    N       : Integer;
    Answer  : word;
begin
  if FreeShip.Edit.RecentFileCount = 0 then exit;
  Filename := Freeship.Edit.RecentFile[0];
  FFilename := Filename;
  if FileExists(Filename)  then
    begin
    FOpenHullWindows;
    Application.ProcessMessages;
    Freeship.Edit.ProgressBar := ProgressBarMain;
    Freeship.Surface.OnFaceRebuilt:=Freeship.Edit.OnFaceRebuilt;
    Freeship.Edit.File_Load(Filename);
    if FreeShip.IsLoadError then
    begin
      Freeship.Edit.ProgressBar := nil;
      Exit;
    end;
    Freeship.RebuildModel;
    Freeship.ZoomFitAllViewports;
    SetCaption;
    UpdateMenu;
    Freeship.Edit.ProgressBar := nil; // temporary turn off until empty VP resolved in Win10
    end;
end;{TMainForm.LoadMostRecentFile}

procedure TMainForm.LoadNamedFile(FileName:string);
var Menu    : TMenuItem;
    N       : Integer;
    Answer  : word;
begin
  if FileExists(Filename)  then
    begin
    Freeship.Edit.ProgressBar := ProgressBarMain;
    Freeship.Surface.OnFaceRebuilt:=Freeship.Edit.OnFaceRebuilt;
    Freeship.Edit.File_Load(Filename);
    if FreeShip.IsLoadError then
    begin
      Freeship.Edit.ProgressBar := nil;
      Exit;
    end;
    Freeship.RebuildModel;
    FOpenHullWindows;
    Freeship.ZoomFitAllViewports;
    SetCaption;
    UpdateMenu;
    end
  else
    begin
       //MessageDlg(rs_Unable_to_open_file_ {UserString[106]}+' '+FFileName,mtError,[mbOk],0);
       FreeEmptyModelChooserDialog:=TFreeEmptyModelChooserDialog.Create(Self);
       if FreeEmptyModelChooserDialog.Execute(FileName)
       then
         begin
         if FreeEmptyModelChooserDialog.RbCreateNew.Checked
           then NewModelExecute(Self)
         else
         if FreeEmptyModelChooserDialog.RbLoadFile.Checked
           then LoadFileExecute(Self)
         end;
       FreeEmptyModelChooserDialog.Free;
    end;
  Freeship.Edit.ProgressBar := nil; // temporary turn off until empty VP resolved in Win10
end;{TMainForm.LoadMostRecentFile}

procedure TMainForm.FreeShipChangeLayerData(Sender: TObject);
var I : Integer;
begin
   // Fill the layerbox with the current layers
   LayerBox.Items.BeginUpdate;
   LayerBox.Items.Clear;
   try
      for I:=0 to Freeship.NumberOfLayers-1 do
      begin
         Layerbox.Items.AddObject(FreeShip.Layer[I].Name,FreeShip.Layer[I]);
      end;
   finally
      LayerBox.Items.EndUpdate;
      I:=LayerBox.Items.IndexOfObject(FreeShip.ActiveLayer);
      Layerbox.ItemIndex:=I;
   end;

  if FreeLayerVisibilityDialog <> nil then
     FreeLayerVisibilityDialog.FillLayers;

end;{TMainForm.FreeShipChangeLayerData}

procedure TMainForm.FreeShipChangeActiveLayer(Sender: TObject;Layer: TFreeSubdivisionLayer);
var Index : Integer;
begin
   if (FreeShip.NumberOfSelectedControlFaces<>0) and (FreeShip.ActiveLayer=Layer) then
   begin
      // do not switch to the active layer when controlfaces are selected
   end else
   begin
      if Layer=nil then
      begin
         Index:=-1;
         Layerbox.ItemIndex:=Index;
         //PanelActiveLayerColor.Color:=clBtnface;
         ColorButton1.ButtonColor:=clBtnface;
      end else
      begin
         Index:=Layerbox.Items.IndexOfObject(Layer);
         Layerbox.ItemIndex:=Index;
         //PanelActiveLayerColor.Color:=Layer.Color;
         ColorButton1.ButtonColor:=Layer.Color;
      end;
   end;
end;{TMainForm.FreeShipChangeActiveLayer}

procedure TMainForm.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  if Action = caFree then
  begin
   FDestroying := true;
   CloseHullWindows;
   FreeShip.Preferences.Save;
   FreeShip.OnChangeActiveLayer:=nil;
   Freeship.OnChangeLayerData:=nil;
   //FreeShip.OnSelectItem:=nil;
   FreeShip.Surface.RemoveOnSelectItemListener(OnSelectItem);
  end;
end;{TMainForm.FormClose}

procedure TMainForm.NewLayerExecute(Sender: TObject);
begin
   FreeShip.Edit.Layer_New;
   UpdateMenu;
end;{TMainForm.NewLayerExecute}

procedure TMainForm.DeleteExecute(Sender: TObject);
begin
   Freeship.Edit.Selection_Delete;
   UpdateMenu;
end;{TMainForm.DeleteExecute}

procedure TMainForm.EdgeCollapseExecute(Sender: TObject);
begin
   FreeShip.Edit.Edge_Collapse;
   UpdateMenu;
end;{TMainForm.EdgeCollapseExecute}

procedure TMainForm.NewEdgeExecute(Sender: TObject);
begin
   FreeShip.Edit.Edge_Connect;
   UpdateMenu;
end;{TMainForm.NewEdgeExecute}

procedure TMainForm.ImportFEFExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ImportFEF;
   FOpenHullWindows;
   SetCaption;
   UpdateMenu;
end;{TMainForm.ImportFEFExecute}

procedure TMainForm.EdgeCreaseExecute(Sender: TObject);
begin
    FreeShip.Edit.Edge_Crease;
    UpdateMenu;
end;{TMainForm.EdgeCreaseExecute}

procedure TMainForm.DeselectAllExecute(Sender: TObject);
begin
   FreeShip.Edit.Selection_Clear;
   UpdateMenu;
end;{TMainForm.ClearSelectionExecute}

procedure TMainForm.PointCollapseExecute(Sender: TObject);
begin
   Freeship.Edit.Point_Collapse;
   UpdateMenu;
end;{TMainForm.PointCollapseExecute}

procedure TMainForm.LayerBoxChange(Sender: TObject);
var Layer   : TFreeSubdivisionLayer;
    I,Index : Integer;
begin
   Index:=Layerbox.ItemIndex;
   if index=-1 then Index:=0;
   Layer:=Layerbox.Items.Objects[index] as TFreeSubdivisionLayer;
   if Freeship.NumberOfSelectedControlFaces=0 then
   begin
      // change the active layer
      if Layer<>FreeShip.ActiveLayer then FreeShip.ActiveLayer:=Layer;
   end else
   begin
      // Assign all selected controlfaces to the new layer
      for I:=FreeShip.NumberOfSelectedControlFaces downto 1 do
         FreeShip.SelectedControlFace[I-1].Layer:=Layer;
      FreeShip.FileChanged:=True;
      FreeShip.Redraw;
   end;
   UpdateMenu;
end;{TMainForm.LayerBoxChange}

procedure TMainForm.PanelActiveLayerColorClick(Sender: TObject);
begin
   ActiveLayerColorExecute(self);
end;{TMainForm.Panel1Click}

procedure TMainForm.ActiveLayerColorExecute(Sender: TObject);
begin
   // change the color of the currently active layer
   ColorDialog.Color:=FreeShip.ActiveLayer.Color;
   if ColorDialog.Execute then
   begin
      FreeShip.ActiveLayer.Color:=ColorDialog.Color;
      FreeShip.FileChanged:=True;
      FreeShip.Redraw;
      FreeShipChangeActiveLayer(self,Freeship.ActiveLayer);
      UpdateMenu;
   end;
end;{TMainForm.ActiveLayerColorExecute}

procedure TMainForm.DeleteEmptyLayersExecute(Sender: TObject);
begin
   Freeship.Edit.Layer_DeleteEmpty(False);
   UpdateMenu;
end;{TMainForm.DeleteEmptyLayersExecute}

procedure TMainForm.LayerDialogExecute(Sender: TObject);
begin
   FreeShip.Edit.Layer_Dialog;
   UpdateMenu;
end;{TMainForm.LayerDialogExecute}

procedure TMainForm.NewModelExecute(Sender: TObject);
begin
   if FreeShip.Edit.Model_New then FOpenHullWindows;
   FreeShip.FileIsReadOnly := false;
   Updatemenu;
end;{TMainForm.NewModelExecute}

procedure TMainForm.FormCreate(Sender: TObject);
begin
   WindowState:=wsMaximized;
   {$IFDEF MEMCHECK}
      // Initialize memcheck, for memory-leak tracking
      MemChk;
   {$ENDIF}
   {$ifndef Windows}
   if self.Align = alTop
     then self.Top := 0;
   {$endif}

   // Removed from LFM, moved here
   FreeShip := TFreeShip.Create(self) ;
   Application.AddOnUserInputHandler(DismissPointEditorOnInput);
   FreeShip.MainForm := self;
   FreeShip.FileChanged := true;
   FreeShip.Filename := 'New model.fbm';
   FreeShip.FileVersion := fv140;
   FreeShip.OnChangeCursorIncrement := FreeShipChangeCursorIncrement;
   FreeShip.OnFileChanged := FreeShipFileChanged;
   FreeShip.OnUpdateGeometryInfo := FreeShipUpdateGeometryInfo;
   FreeShip.OnUpdateRecentFileList := FreeShipUpdateRecentFileList;
   FreeShip.OnUpdateUndoData := FreeShipUpdateUndoData;
   FreeShip.Precision := fpLow;

   FAllToolbarsControlsWidth := 0;
   GlobalFreeship := Freeship;
   FModelInitallyLoaded := false;
   CreateStudioBar;
   //dumpIcons;
   //FShowSplash := true;
end;{TMainForm.FormCreate}

procedure TMainForm.ShowStationsExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowStations:=not FreeShip.Visibility.ShowStations;
   UpdateMenu;
end;{TMainForm.ShowStationsExecute}

procedure TMainForm.ShowButtocksExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowButtocks:=not FreeShip.Visibility.ShowButtocks;
   UpdateMenu;
end;{TMainForm.ShowButtocksExecute}

procedure TMainForm.ShowWaterlinesExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowWaterlines:=not FreeShip.Visibility.ShowWaterlines;
   UpdateMenu;
end;{TMainForm.ShowWaterlinesExecute}

resourcestring
 rsExitConfirmation = 'The current model has been changed'+#10
   +'Are you sure you want to exit?';

procedure TMainForm.FormCloseQuery(Sender: TObject; var CanClose: Boolean);
var Answer:word;
begin
   if Freeship.FileChanged then
   begin
      //Answer:=MessageDlg(rs_The_current_model_has_been_changed_ {UserString[103]}+EOL+rs_Are_you_sure_you_want_to_quit {UserString[282]}+'?',mtWarning,[mbYes,mbNo],0);
   Answer:=MessageDlg(rsExitConfirmation, mtWarning, [mbNo, mbYes],0,mbNo);
   CanClose:=Answer=mrYes;
   end;
end;{TMainForm.FormCloseQuery}

procedure TMainForm.NewFaceExecute(Sender: TObject);
begin
   FreeShip.Edit.Face_New;
   UpdateMenu;
end;{TMainForm.NewFaceExecute}

procedure TMainForm.IntersectionDialogExecute(Sender: TObject);
begin
   FreeShip.Edit.Intersection_Dialog;
   UpdateMenu;
end;{TMainForm.IntersectionDialogExecute}

procedure TMainForm.EdgeExtrudeExecute(Sender: TObject);
begin
   FreeShip.Edit.Edge_Extrude;
   UpdateMenu;
end;{TMainForm.EdgeExtrudeExecute}

procedure TMainForm.About1Click(Sender: TObject);
var FreeAboutDlg : TFreeAboutDlg;
begin
 FreeAboutDlg := TFreeAboutDlg.Create(Self);
 FreeAboutDlg.ShowModal;
 FreeAndNil(FreeAboutDlg);
end;{TMainForm.About1Click}


// end correction Victor T

procedure TMainForm.EdgeSplitExecute(Sender: TObject);
begin
   FreeShip.Edit.Edge_Split;
   UpdateMenu;
end;{TMainForm.EdgeSplitExecute}

procedure TMainForm.ExportFEFExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ExportFEF;
   UpdateMenu;
end;{TMainForm.ExportFEFExecute}

procedure TMainForm.EditProjectSettingsExecute(Sender: TObject);
begin
   FreeShip.ProjectSettings.Edit;
   UpdateMenu;
end;{TMainForm.EditProjectSettingsExecute}

procedure TMainForm.CheckModelExecute(Sender: TObject);
begin
   FreeShip.Edit.Model_Check(True);
   UpdateMenu;
end;{TMainForm.AnalyzeSurfaceExecute}

procedure TMainForm.ShowNormalsExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowNormals:=not FreeShip.Visibility.ShowNormals;
   UpdateMenu;
end;{TMainForm.ShowNormalsExecute}

procedure TMainForm.DesignHydrostaticsExecute(Sender: TObject);
var Calculation : TFreeHydrostaticCalc;
begin
   Calculation:=Freeship.Edit.Hydrostatics_Calculate(Freeship.ProjectSettings.ProjectDraft,0.0,0.0);
   if Calculation<>nil then
   begin
      FreeAndNil(Calculation);
   end;
end;{TMainForm.DesignHydrostaticsExecute}

procedure TMainForm.ImportVRMLExecute(Sender: TObject);
//var DateTime : TDateTime;
//    str_1,str_2 : string;
begin
   //   DateTime := Time;  // store the current date and time
   //   str_1 := TimeToStr(DateTime); // convert the time into a string
   FreeShip.Edit.File_ImportVRML;
   FOpenHullWindows;
   Application.ProcessMessages;
   FreeShip.ZoomFitAllViewports;
   //   DateTime := Time;  // store the current date and time
   //   str_2 := TimeToStr(DateTime); // convert the time into a string
   //   MessageDlg(('Time import='+str_2+' - '+str_1),mtInformation,[mbOK],0);
   UpdateMenu;
end;{TMainForm.ImportVRMLExecute}

procedure TMainForm.RemoveNegativeExecute(Sender: TObject);
begin
   Freeship.Edit.Face_DeleteNegative;
   UpdateMenu;
end;{TMainForm.RemoveNegativeExecute}

procedure TMainForm.RotateModelExecute(Sender: TObject);
begin
   FreeShip.Edit.Face_Rotate;
   UpdateMenu;
end;{TMainForm.RotateModelExecute}

procedure TMainForm.RotateModelMExecute(Sender: TObject);
begin
   FreeShip.Edit.Face_RotateM;
   UpdateMenu;
end;{TMainForm.RotateModelMExecute}

procedure TMainForm.ScaleModelExecute(Sender: TObject);
begin
   FreeShip.Edit.Face_Scale;
   UpdateMenu;
end;{TMainForm.ScaleModelExecute}

procedure TMainForm.ShowGridExecute(Sender: TObject);
begin
   Freeship.Visibility.ShowGrid:=not Freeship.Visibility.ShowGrid;
   UpdateMenu;
end;{TMainForm.ShowGridExecute}

procedure TMainForm.ToolButtonSelectClick(Sender: TObject);
begin
end;

procedure TMainForm.UndoExecute(Sender: TObject);
begin
   FreeShip.Edit.Undo;
   UpdateMenu;
   SetCaption;
end;{TMainForm.UndoExecute}

   // Update undo memory usage
procedure TMainForm.FreeShipUpdateUndoData(Sender: TObject);
var Memory : Integer;
begin
   Memory:=Trunc(Freeship.UndoMemory/1024);
   if Memory<1024 then LabelUndoMemory.Caption:=rs_Undo_memory {UserString[283]}+' : '+IntToStr(Memory)+' Kb'
                  else LabelUndoMemory.Caption:=rs_Undo_memory {UserString[283]}+' : '+FloatToDec(Memory/1024,3)+' Mb.';
   Undo.Enabled:=FreeShip.UndoCount>0;
   SetCaption;
   UpdateMenu;
end;{TMainForm.FreeShipUpdateUndoData}

procedure TMainForm.HydrostaticsDialogExecute(Sender: TObject);
begin
   Freeship.Edit.Hydrostatics_Dialog;
   UpdateMenu;
end;{TMainForm.HydrostaticsDialogExecute}

procedure TMainForm.ExportObjExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ExportObj;
   UpdateMenu;
end;{TMainForm.ExportObjExecute}

procedure TMainForm.InvertFaceExecute(Sender: TObject);
begin
   Freeship.Edit.Face_Flip;
   UpdateMenu;
end;{TMainForm.InvertFaceExecute}

procedure TMainForm.PreferencesExecute(Sender: TObject);
begin
   FreeShip.Preferences.Edit;
   RefreshStudioLanguage;
   UpdateMenu;
   LoadToolIcons;
end;{TMainForm.PreferencesExecute}

procedure TMainForm.ImportBodyplanExecute(Sender: TObject);
begin
   FreeShip.Edit.ImportFrames;
   FOpenHullWindows;
   SetCaption;
   UpdateMenu;
end;{TMainForm.ImportBodyplanExecute}

procedure TMainForm.ExportDXF3DPolylinesExecute(Sender: TObject);
begin
   Freeship.Edit.File_ExportDXF_3DPolylines;
   UpdateMenu;
end;{TMainForm.ExportDXFPolylinesExecute}

procedure TMainForm.ExportDXFFacesExecute(Sender: TObject);
begin
   Freeship.Edit.File_ExportDXF_Faces;
   UpdateMenu;
end;{TMainForm.ExportDXFFacesExecute}

procedure TMainForm.ImportHullFileExecute(Sender: TObject);
begin
   Freeship.Edit.File_ImportHull;
   FOpenHullWindows;
   UpdateMenu;
end;{TMainForm.ImportHullFileExecute}

procedure TMainForm.ExportOffsetsExecute(Sender: TObject);
begin
   Freeship.Edit.File_ExportOffsets;
   UpdateMenu;
end;{TMainForm.ExportOffsetsExecute}

procedure TMainForm.MoveModelExecute(Sender: TObject);
begin
   FreeShip.Edit.Face_Move;
   UpdateMenu;
end;{TMainForm.MoveModelExecute}

procedure TMainForm.AddPointExecute(Sender: TObject);
begin
   if Freeship.EditMode = emAddPoint then
     Freeship.EditMode := emSelectItems
   else
     Freeship.EditMode := emAddPoint;
   UpdateMenu;
end;{TMainForm.AddPointExecute}

procedure TMainForm.DevelopLayersExecute(Sender: TObject);
begin
   Screen.Cursor:= crHourglass;
   Application.ProcessMessages;

   FreeShip.Edit.Layer_Develop;

   Screen.Cursor:= crDefault;

   UpdateMenu;
end;{TMainForm.DevelopLayersExecute}

procedure TMainForm.ExportArchimedesExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ExportArchimedes;
   UpdateMenu;
end;{TMainForm.ExportArchimedesExecute}

procedure TMainForm.ShowLinesplanExecute(Sender: TObject);
var I          : Integer;
    AlreadyOpen: Boolean;
    Form       : TFreeLinesplanForm;
begin
   if not Freeship.ProjectSettings.MainParticularsHasBeenset then
   begin
      MessageDlg(rs_You_have_to_set_the_mainparticulars_first_ {UserString[96]},mtWarning,[mbOk],0);
      exit;
   end;
   AlreadyOpen:=False;
   for I:=0 to Screen.FormCount-1 do if (Screen.Forms[I] is TFreeLinesplanForm) and
     TFreeLinesplanForm(Screen.Forms[I]).DisplaysShip(FreeShip) then
   begin
      AlreadyOpen:=True;
      if Screen.Forms[I].WindowState=wsMinimized then Screen.Forms[I].WindowState:=wsNormal;
      Screen.Forms[I].BringToFront;
      break;
   end;
   if not AlreadyOpen then
   begin
      Form:=TFreeLinesplanForm.Create(self);
      //ShowTranslatedValues(Form.LinesplanFrame);
      //ShowTranslatedValues(Form);
      Form.OpenForShip(FreeShip);
   end;
end;{TMainForm.ShowLinesplanExecute}

procedure TMainForm.ShowDiagonalsExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowDiagonals:=not FreeShip.Visibility.ShowDiagonals;
   UpdateMenu;
end;{TMainForm.ShowDiagonalsExecute}

procedure TMainForm.FreeShipUpdateRecentFileList(Sender: TObject);
var I    : Integer;
    Item : TMenuItem;
begin
   // delete old menu items
   RecentFiles.Clear;
   // add the new data
   for I:=1 to FreeShip.Edit.RecentFileCount do
   begin
      Item:=TMenuItem.Create(self);
      Item.Caption:=Freeship.Edit.RecentFile[I-1];
      Item.OnClick:=FLoadRecentFile;
      RecentFiles.Add(Item);
   end;
   UpdateMenu;
end;{TMainForm.FreeShipUpdateRecentFileList}

procedure TMainForm.ImportCareneExecute(Sender: TObject);
begin
   Freeship.Edit.File_ImportCarene;
   FOpenHullWindows;
   UpdateMenu;
end;{TMainForm.ImportCareneExecute}

procedure TMainForm.ShowMarkersExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowMarkers:=not FreeShip.Visibility.ShowMarkers;
   UpdateMenu;
end;{TMainForm.ShowMarkersExecute}

procedure TMainForm.DeleteMarkersExecute(Sender: TObject);
begin
   Freeship.Edit.Marker_Delete;
   UpdateMenu;
end;{TMainForm.DeleteMarkersExecute}

procedure TMainForm.ImportSurfaceExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ImportSurface;
   FOpenHullWindows;
   SetCaption;
   UpdateMenu;
end;{TMainForm.ImportSurfaceExecute}

procedure TMainForm.ShowcurvatureExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowCurvature:=not FreeShip.Visibility.ShowCurvature;
   UpdateMenu;
end;{TMainForm.ShowcurvatureExecute}

procedure TMainForm.IncreaseCurvatureScaleExecute(Sender: TObject);
begin
   Freeship.Visibility.IncreaseCurvatureScale;
end;{TMainForm.IncreaseCurvatureScaleExecute}

procedure TMainForm.DecreaseCurvatureScaleExecute(Sender: TObject);
begin
   Freeship.Visibility.DecreaseCurvatureScale;
end;{TMainForm.DecreaseCurvatureScaleExecute}

procedure TMainForm.FileSaveExecute(Sender: TObject);
begin
  if Freeship.FilenameSet and not Freeship.FileIsReadOnly then
     FreeShip.Edit.File_Save
  else
     FreeShip.Edit.File_SaveAs;
   UpdateMenu;
end;{TMainForm.FileSaveExecute}

procedure TMainForm.ImportChinesExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ImportChines;
   FOpenHullWindows;
   SetCaption;
   UpdateMenu;
end;{TMainForm.ImportChinesExecute}

procedure TMainForm.ShowControlCurvesExecute(Sender: TObject);
begin
   FreeShip.Visibility.ShowControlCurves:=not FreeShip.Visibility.ShowControlCurves;
   UpdateMenu;
end;{TMainForm.ShowControlCurvesExecute}

procedure TMainForm.NewCurveExecute(Sender: TObject);
begin
   Freeship.Edit.Curve_Add;
   UpdateMenu;
end;{TMainForm.AddCurveExecute}

procedure TMainForm.ExportCoordinatesExecute(Sender: TObject);
begin
   Freeship.Edit.File_ExportCoordinates;
   UpdateMenu;
end;{TMainForm.ExportCoordinatesExecute}

procedure TMainForm.InsertPlaneExecute(Sender: TObject);
begin
   Freeship.Edit.Point_InsertPlane;
   UpdateMenu;
end;{TMainForm.InsertPlaneExecute}

procedure TMainForm.PointsLockExecute(Sender: TObject);
begin
   Freeship.Edit.Point_Lock;
   UpdateMenu;
end;{TMainForm.PointsLockExecute}

procedure TMainForm.PointsUnlockExecute(Sender: TObject);
begin
   Freeship.Edit.Point_Unlock;
   UpdateMenu;
end;{TMainForm.UnlockPointsExecute}

procedure TMainForm.PointsUnlockAllExecute(Sender: TObject);
begin
   Freeship.Edit.Point_UnlockAll;
   UpdateMenu;
end;{TMainForm.PointsUnlockAllExecute}

procedure TMainForm.ImportMarkersExecute(Sender: TObject);
begin
   Freeship.Edit.Marker_Import;
   UpdateMenu;
end;{TMainForm.ImportMarkersExecute}

procedure TMainForm.ExportMichletExecute(Sender: TObject);
begin
   Freeship.Edit.File_Export_Michlet;
   UpdateMenu;
end;{TMainForm.ExportMichletExecute}

procedure TMainForm.ExportAddMassExecute(Sender: TObject);
begin
   Freeship.Edit.File_Export_AddMass;
   UpdateMenu;
end;{TMainForm.ExportAddMassExecute}

procedure TMainForm.PropTask1Execute(Sender: TObject);
begin
   Freeship.Edit.Propeller_Task1;
   Updatemenu;
end;{TMainForm.PropTask1Execute}

procedure TMainForm.PropTask2Execute(Sender: TObject);
begin
   Freeship.Edit.Propeller_Task2;
   Updatemenu;
end;{TMainForm.PropTask2Execute}

procedure TMainForm.PropTask3Execute(Sender: TObject);
begin
   Freeship.Edit.Propeller_Task3;
   Updatemenu;
end;{TMainForm.PropTask3Execute}

procedure TMainForm.PropTask4Execute(Sender: TObject);
begin
   Freeship.Edit.Propeller_Task4;
   Updatemenu;
end;{TMainForm.PropTask4Execute}

procedure TMainForm.PropTask5Execute(Sender: TObject);
begin
   Freeship.Edit.Propeller_Task5;
   Updatemenu;
end;{TMainForm.PropTask5Execute}

procedure TMainForm.PropTaskRvrsExecute(Sender: TObject);
begin
   Freeship.Edit.Hydrodyn_Rvrs;
   Updatemenu;
end;{TMainForm.PropTaskRvrsExecute}

procedure TMainForm.HydrodynManeuvExecute(Sender: TObject);
begin
   Freeship.Edit.Hydrodyn_Maneuv;
   Updatemenu;
end;{TMainForm.HydrodynManeuvExecute}

//procedure TMainForm.HydrodynTask0Execute(Sender: TObject);
//begin
//   Freeship.Edit.Hydrodyn_Task1; // added masses
//   Updatemenu;
//end;{TMainForm.HydrodynTask0Execute}

procedure TMainForm.HydrodynTask1Execute(Sender: TObject);
begin
   Freeship.Edit.Hydrodyn_Task1; // Aerodynamic characteristics
   Updatemenu;
end;{TMainForm.HydrodynTask1Execute}

procedure TMainForm.HydrodynTask2Execute(Sender: TObject);
begin
//   Freeship.Edit.Hydrodyn_Task2; // Positional characteristics
   Updatemenu;
end;{TMainForm.HydrodynTask2Execute}

procedure TMainForm.HydrodynTask3Execute(Sender: TObject);
begin
//   Freeship.Edit.Hydrodyn_Task3;  // Rotative characteristics
   Updatemenu;
end;{TMainForm.HydrodynTask3Execute}

procedure TMainForm.HydrodynTask4Execute(Sender: TObject);  //для расчета присоединенных масс
begin
   Updatemenu;
end;{TMainForm.HydrodynTask4Execute}

procedure TMainForm.ResistanceKaperExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_Kaper;
   Updatemenu;
end;{TMainForm.ResistanceKaperExecute}

procedure TMainForm.ResistancePlaningExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_Planing;
   Updatemenu;
end;{TMainForm.ResistancePlaningExecute}


procedure TMainForm.ResistanceDelftExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_Delft;
   Updatemenu;
end;{TMainForm.ResistanceDelftExecute}

procedure TMainForm.ResistanceHoltrExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_Holtr;
   Updatemenu;
end;{TMainForm.ResistanceHoltrExecute}

procedure TMainForm.ResistanceOSTExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_OST;
   Updatemenu;
end;{TMainForm.ResistanceOSTExecute}

procedure TMainForm.ResistanceRBHSExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_RBHS;
   Updatemenu;
end;{TMainForm.ResistanceRBHSExecute}

procedure TMainForm.ResistanceMHExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_MH;
   Updatemenu;
end;{TMainForm.ResistanceMHExecute}

procedure TMainForm.ResistanceHollenExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_Hollen;
   Updatemenu;
end;{TMainForm.ResistanceHollenExecute}

procedure TMainForm.ResistanceOortmerExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_Oortmer;
   Updatemenu;
end;{TMainForm.ResistanceOortmerExecute}

procedure TMainForm.ResistanceFungLeibExecute(Sender: TObject);
begin
   Freeship.Edit.Resistance_FungLeib;
   Updatemenu;
end;{TMainForm.ResistanceFungLeibExecute}

procedure TMainForm.FreeShipChangeCursorIncrement(Sender: TObject);
begin
  if (csdestroying in componentstate) then exit;
  LabelDistance.Caption:=rs_Incr__distance {UserString[284]}+': '+FloatToDec(Freeship.Visibility.CursorIncrement,7);
end;{TMainForm.FreeShipChangeCursorIncrement}

procedure TMainForm.StatusPanel3Click(Sender: TObject);
var Str  : Ansistring;
    I    : integer;
    Value: TFloatType;
begin
   if Freeship.Surface.NumberOfControlPoints=0 then exit;
   Str:=FloatToDec(Freeship.Visibility.CursorIncrement,5);
   if InputQuery('',rs_New_increment_distance {UserString[285]}+':',Str) then
   begin
      Val(Str,Value,I);
      if I=0 then Freeship.Visibility.CursorIncrement:=Value;
   end;
end;{TMainForm.Panel3Click}

procedure TMainForm.PointAlignExecute(Sender: TObject);
begin
   Freeship.Edit.Point_ProjectStraightLine;
   UpdateMenu;
end;{TMainForm.PointProjectLineExecute}

procedure TMainForm.PointAlighnPermanentlyExecute(Sender: TObject);
begin
   Freeship.Edit.Point_ProjectStraightLinePermanentConstraint;
   UpdateMenu;
end;{TMainForm.PointProjectLineExecute}

procedure TMainForm.PointAnchorExecute(Sender: TObject);
begin
   Freeship.Edit.Point_AnchorToPoint;
   UpdateMenu;
end;{TMainForm.PointProjectLineExecute}

procedure TMainForm.SelectAllControlPoints1Click(Sender: TObject);
begin
end;

procedure TMainForm.SelectAllControlPointsExecute(Sender: TObject);
begin
  Freeship.Edit.Selection_SelectAllControlPoints;
  UpdateMenu;
end;

procedure TMainForm.ImportMichletWavesExecute(Sender: TObject);
begin
   Freeship.Edit.File_Import_MichletWaves;
   UpdateMenu;
end;{TMainForm.ImportMichletWavesExecute}

procedure TMainForm.ShowHydrostaticsExecute(Sender: TObject);
begin
   Freeship.Visibility.ShowHydrostaticData:=not Freeship.Visibility.ShowHydrostaticData;
   updatemenu;
end;{TMainForm.ShowHydrostaticsExecute}

procedure TMainForm.MirrorFaceExecute(Sender: TObject);
begin
   Freeship.Edit.Face_MirrorPlane;
   UpdateMenu;
end;{TMainForm.MirrorFaceExecute}

procedure TMainForm.ExportDXF2DPolylinesExecute(Sender: TObject);
begin
   Freeship.Edit.File_ExportDXF_2DPolylines;
   UpdateMenu;
end;{TMainForm.ExportDXF2DPolylinesExecute}

procedure TMainForm.FreeShipUpdateGeometryInfo(Sender: TObject);
begin
   LabelNumbers.Caption:=IntToStr(Freeship.Surface.NumberOfControlFaces)+#32+rs_faces {UserString[286]}+', '+
                   IntToStr(Freeship.Surface.NumberOfControlEdges)+#32+rs_edges {UserString[287]}+', '+
                   IntToStr(Freeship.Surface.NumberOfControlPoints)+#32+rs_points {UserString[288]}+', '+
                   IntToStr(Freeship.Surface.NumberOfControlCurves)+#32+rs_curves {UserString[289]};
   if Freeship.Surface.Changed then
     UpdateMenu;
end;{TMainForm.FreeShipUpdateGeometryInfo}

procedure TMainForm.TransformLackenbyExecute(Sender: TObject);
begin
   Freeship.Edit.Model_LackenbyTransformation;
   UpdateMenu;
end;{TMainForm.TransformLackenbyExecute}

procedure TMainForm.ExportIGESExecute(Sender: TObject);
begin
   Freeship.Edit.File_ExportIGES;
   UpdateMenu;
end;{TMainForm.ExportIGESExecute}

procedure TMainForm.ExportPartExecute(Sender: TObject);
begin
   Freeship.Edit.File_ExportPart;
   UpdateMenu;
end;{TMainForm.ExportPartExecute}

procedure TMainForm.ImportPartExecute(Sender: TObject);
begin
   Freeship.Edit.File_ImportPart;
   UpdateMenu;
end;{TMainForm.ImportPartExecute}

procedure TMainForm.LayerIntersectionExecute(Sender: TObject);
begin
   Freeship.Edit.Point_IntersectLayer;
   UpdateMenu;
end;{TMainForm.LayerIntersectionExecute}

procedure TMainForm.KeelRudderWizardExecute(Sender: TObject);
begin
   if not Assigned(FreeKeelWizardDialog) then
     FreeKeelWizardDialog := TFreeKeelWizardDialog.Create(Self);
   //ShowTranslatedValues(FreeKeelWizardDialog);
   FreeKeelWizardDialog.Execute(freeship);
   UpdateMenu;
end;{TMainForm.KeelRudderWizardExecute}

procedure TMainForm.RedoExecute(Sender: TObject);
begin
   FreeShip.Edit.Redo;
   UpdateMenu;
   SetCaption;
end;{TMainForm.RedoExecute}

procedure TMainForm.ClearUndoExecute(Sender: TObject);
begin
   Freeship.Edit.Undo_Clear;
   UpdateMenu;
end;{TMainForm.ClearUndoExecute}

procedure TMainForm.ShowUndoHistoryExecute(Sender: TObject);
begin
   Freeship.Edit.Undo_ShowHistory;
   UpdateMenu;
end;{TMainForm.ShowUndoHistoryExecute}

procedure TMainForm.ImportPolyCadExecute(Sender: TObject);
begin
   Freeship.Edit.File_ImportPolycad;
   FOpenHullWindows;
   UpdateMenu;
end;{TMainForm.ImportPolyCadExecute}

procedure TMainForm.ImportSTLExecute(Sender: TObject);
begin
  Freeship.Edit.File_ImportSTL;
  FOpenHullWindows;
  UpdateMenu;
end;

procedure TMainForm.RemoveUnusedPointsExecute(Sender: TObject);
begin
   Freeship.Edit.Point_RemoveUnused;
   Updatemenu;
end;{TMainForm.RemoveUnusedPointsExecute}

procedure TMainForm.ExportGHSExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ExportGHS;
   UpdateMenu;
end;{TMainForm.ExportGHSExecute}

procedure TMainForm.ExportPAMExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ExportPAM;
   UpdateMenu;
end;{TMainForm.ExportPAMExecute}

procedure TMainForm.ShowFlowlinesExecute(Sender: TObject);
begin
   Freeship.Visibility.ShowFlowlines:=not Freeship.Visibility.ShowFlowlines;
   updatemenu;
end;{TMainForm.ShowFlowlinesExecute}

procedure TMainForm.AddCylinderExecute(Sender: TObject);
begin
   Freeship.Edit.Geometry_AddCylinder;
   UpdateMenu;
end;{TMainForm.AddCylinderExecute}

procedure TMainForm.SelectAllExecute(Sender: TObject);
begin
   Freeship.Edit.Selection_ToggleAll;
   UpdateMenu;
end;{TMainForm.SelectAllExecute}

procedure TMainForm.ExportSTLExecute(Sender: TObject);
begin
   FreeShip.Edit.File_ExportSTL;
   UpdateMenu;
end;{TMainForm.ExportSTLExecute}

procedure TMainForm.CrossCurvesExecute(Sender: TObject);
begin
   Freeship.Edit.Hydrostatics_Crosscurves;
   UpdateMenu;
end;{TMainForm.CrossCurvesExecute}

procedure TMainForm.SelectLeakPointsExecute(Sender: TObject);
begin
  Freeship.Edit.Selection_SelectLeakPoints;
  UpdateMenu;
end;

{ this is used just once to dump menu/toolbutton icons }
procedure TMainForm.dumpIcons;
begin
  Freeship.Preferences.dumpIcons(MenuImages,ActionList1);
end;

// loads icons from FMenuIconDirectory that is set according to theme and icon size
procedure TMainForm.LoadToolIcons;
var i, II, sz, w, ww, ilcnt:integer;
    A: TAction; AName, IName, IPath, IconFile:String;
    cil: TCustomImageList;
    bmp:TBitmap; png: TPortableNetworkGraphic; img: TLazIntfImage;

  procedure orderToolButtons(ToolBar: TToolBar);
  var i,l,sz:integer; tb:TControl; n:string;
      tba : array of TControl;
      needsReinsert:boolean = false;
  begin
    sz:=ToolBar.ControlCount;
    setLength(tba, sz);
    w := 4;
    if ToolBar.Name = 'ToolBarLayers'
      then exit;
    for i:=0 to sz-1 do
    begin
      tb:=ToolBar.Controls[i];
      tba[tb.Tag] := tb;
      if tb.Tag<>i then needsReinsert := true;
      //tb.Hint:= inttostr(tb.Tag) +' '+ tb.Hint;
      w := w + tb.Width + 1;
    end;

    if needsReinsert then
    for i:=0 to sz-1 do
      ToolBar.RemoveControl(tba[i]);

    ww:=ToolBar.Width;
    ToolBar.Width := w + 4;
    w := 4;
    for i:=0 to sz-1 do
    begin
      tb:=tba[i];
      tb.Left := w;
      if tb.name='LayerBoxPanel'
        then w:=w;
      if needsReinsert then
         ToolBar.InsertControl(tba[i],i);
      w := w + tb.Width;
    end;
    ToolBar.Width:=w;
    setLength(tba, 0);
  end;

  procedure setToolBarButtonSize(ToolBar: TToolBar; sz: integer);
    var i,l:integer; tb:TToolButton; c:TControl; n:string;
    begin
      ToolBar.ButtonHeight:= sz + (sz div 16)*2;
      ToolBar.ButtonWidth := sz + (sz div 16)*2;
      for i:=0 to ToolBar.ControlCount-1 do
        begin
          c:=ToolBar.Controls[i];
          n:=c.Name;
          l:=c.Left;
          if c is TToolButton then
             begin
             c.Height := ToolBar.ButtonHeight;
             c.Width := ToolBar.ButtonWidth;
             end
          else if c is TColorButton then
             begin
             c.Height := ToolBar.ButtonHeight;
             c.Width := ToolBar.ButtonWidth;
             end
          else if c is TPanel then
             begin
             c.Height := ToolBar.ButtonHeight;
             c.Width := ToolBar.ButtonWidth;
             end
          else
             begin
             c.Constraints.MinWidth := ToolBar.ButtonWidth;
             end;
        end;
      orderToolButtons(ToolBar);
    end;


begin
  FreeShip.preferences.LoadImageListByActions(MenuImages, ActionList1);

  sz := Freeship.Preferences.ToolIconSize;

  setToolBarButtonSize(ToolBarFile, sz);
  setToolBarButtonSize(ToolBarVisibility, sz);
  setToolBarButtonSize(ToolBarLayers, sz);
  setToolBarButtonSize(ToolBarPoints, sz);
  setToolBarButtonSize(ToolBarEdit, sz);
  setToolBarButtonSize(ToolBarEdges, sz);
  setToolBarButtonSize(ToolBarFaces, sz);
  setToolBarButtonSize(ToolBarCurves, sz);

  FAllToolbarsControlsWidth := getAllToolbarsControlWidth;
  //AlignAllToolbars;
  ArrangeRibbonPanel(PanelMain);

  {
  // this is just to kick toolbar to autoresize
  Self.Height:=Self.Height+1;
  Self.Resize;
  Self.Height:=Self.Height-1;
  Self.Resize;
  Self.Invalidate;
  }
end;

//initialization
  //ExceptionDlg := TExceptionDlg.create(nil);
end.
