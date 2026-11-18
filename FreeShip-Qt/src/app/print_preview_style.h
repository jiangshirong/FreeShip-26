#pragma once
class QPrintPreviewDialog;
// Restyle Qt's existing preview controls, retaining its PrintSupport callbacks.
void styleStudioPrintPreview(QPrintPreviewDialog* dialog,bool allowMultiPage=false);
