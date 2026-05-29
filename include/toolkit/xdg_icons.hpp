// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

namespace XDG {

namespace IconContexts {
inline constexpr const char *actions = "actions";
inline constexpr const char *animations = "animations";
inline constexpr const char *applications = "apps";
inline constexpr const char *categories = "categories";
inline constexpr const char *devices = "devices";
inline constexpr const char *emblems = "emblems";
inline constexpr const char *emotes = "emotes";
inline constexpr const char *international = "intl";
inline constexpr const char *mimeTypes = "mimetypes";
inline constexpr const char *places = "places";
inline constexpr const char *status = "status";
} // namespace IconContexts

namespace IconActions {
inline constexpr const char *addressBookNew = "address-book-new";
inline constexpr const char *applicationExit = "application-exit";
inline constexpr const char *appointmentNew = "appointment-new";
inline constexpr const char *callStart = "call-start";
inline constexpr const char *callStop = "call-stop";

inline constexpr const char *editCopy = "edit-copy";
inline constexpr const char *editCut = "edit-cut";
inline constexpr const char *editDelete = "edit-delete";
inline constexpr const char *editFind = "edit-find";
inline constexpr const char *editFindReplace = "edit-find-replace";
inline constexpr const char *editPaste = "edit-paste";
inline constexpr const char *editRedo = "edit-redo";
inline constexpr const char *editSelectAll = "edit-select-all";
inline constexpr const char *editUndo = "edit-undo";

inline constexpr const char *findLocation = "find-location";
inline constexpr const char *folderNew = "folder-new";

// For navigating to the user's home directory
inline constexpr const char *goHome = "go-home";
// For navigating up one directory level
inline constexpr const char *goUp = "go-up";

// For switching to icon/grid view mode
inline constexpr const char *viewListIconsSymbolic = "view-list-icons-symbolic";
// For switching to compact list view mode
inline constexpr const char *viewListCompactSymbolic = "view-list-compact-symbolic";
// For switching to detailed list view mode (name, size, date)
inline constexpr const char *viewListDetailsSymbolic = "view-list-details-symbolic";

// For toggling visibility of hidden files
inline constexpr const char *showHiddenFiles = "show-hidden-files";
// For filtering or refining visible content
inline constexpr const char *viewFilter = "view-filter";

inline constexpr const char *formatIndentLess = "format-indent-less";
inline constexpr const char *formatIndentMore = "format-indent-more";
inline constexpr const char *formatJustifyCenter = "format-justify-center";
inline constexpr const char *formatJustifyFill = "format-justify-fill";
inline constexpr const char *formatJustifyLeft = "format-justify-left";
inline constexpr const char *formatJustifyRight = "format-justify-right";

inline constexpr const char *formatTextDirectionLtr = "format-text-direction-ltr";
inline constexpr const char *formatTextDirectionRtl = "format-text-direction-rtl";
inline constexpr const char *formatTextBold = "format-text-bold";
inline constexpr const char *formatTextItalic = "format-text-italic";

inline constexpr const char *insertObject = "insert-object";
inline constexpr const char *insertText = "insert-text";

inline constexpr const char *listAdd = "list-add";
inline constexpr const char *listRemove = "list-remove";

inline constexpr const char *mailForward = "mail-forward";
inline constexpr const char *mailMarkImportant = "mail-mark-important";
inline constexpr const char *mailMarkJunk = "mail-mark-junk";
inline constexpr const char *mailMarkNotJunk = "mail-mark-notjunk";
inline constexpr const char *mailMarkRead = "mail-mark-read";
inline constexpr const char *mailMarkUnread = "mail-mark-unread";
inline constexpr const char *mailMessageNew = "mail-message-new";
inline constexpr const char *mailReplyAll = "mail-reply-all";
inline constexpr const char *mailReplySender = "mail-reply-sender";
inline constexpr const char *mailSend = "mail-send";

// Are not strictly in the docs, but in actual themes
inline constexpr const char *documentExport = "document-export.png";
inline constexpr const char *documentImport = "document-import.png";
inline constexpr const char *documentNew = "document-new.png";
inline constexpr const char *documentOpen = "document-open.png";
inline constexpr const char *documentRecent = "document-open-recent.png";
inline constexpr const char *documentPageSetup = "document-page-setup.png";
inline constexpr const char *documentPrint = "document-print.png";
inline constexpr const char *documentPrintPreview = "document-print-preview.png";
inline constexpr const char *documentProperties = "document-properties.png";
inline constexpr const char *documentRevert = "document-revert.png";
inline constexpr const char *documentSave = "document-save.png";
inline constexpr const char *documentSaveAs = "document-save-as.png";
inline constexpr const char *documentSend = "document-send.png";

inline constexpr const char *dialogApply = "dialog-apply";
inline constexpr const char *dialogNo = "dialog-no.png";
inline constexpr const char *dialogOk = "dialog-ok.png";
inline constexpr const char *dialogYes = "dialog-yes.png";
} // namespace IconActions

namespace IconApplications {
inline constexpr const char *accessoriesCalculator = "accessories-calculator";
inline constexpr const char *accessoriesTextEditor = "accessories-text-editor";
inline constexpr const char *helpBrowser = "help-browser";
inline constexpr const char *internetWebBrowser = "internet-web-browser";
inline constexpr const char *multimediaVolumeControl = "multimedia-volume-control";
inline constexpr const char *officeCalendar = "office-calendar";
inline constexpr const char *preferencesSystem = "preferences-system";
inline constexpr const char *systemFileManager = "system-file-manager";
inline constexpr const char *systemSoftwareUpdate = "system-software-update";
inline constexpr const char *utilitiesTerminal = "utilities-terminal";
} // namespace IconApplications

namespace IconStatus {
inline constexpr const char *dialogError = "dialog-error";
inline constexpr const char *dialogInformation = "dialog-information";
inline constexpr const char *dialogWarning = "dialog-warning";
inline constexpr const char *dialogQuestion = "dialog-question";

inline constexpr const char *batteryCaution = "battery-caution";
inline constexpr const char *batteryLow = "battery-low";
inline constexpr const char *batteryFull = "battery-full";

inline constexpr const char *networkOffline = "network-offline";
inline constexpr const char *networkIdle = "network-idle";
inline constexpr const char *networkTransmit = "network-transmit";
inline constexpr const char *networkReceive = "network-receive";
} // namespace IconStatus

namespace IconCategories {
inline constexpr const char *applicationsAccessories = "applications-accessories";
inline constexpr const char *applicationsDevelopment = "applications-development";
inline constexpr const char *applicationsEngineering = "applications-engineering";
inline constexpr const char *applicationsGames = "applications-games";
inline constexpr const char *applicationsGraphics = "applications-graphics";
inline constexpr const char *applicationsInternet = "applications-internet";
inline constexpr const char *applicationsMultimedia = "applications-multimedia";
inline constexpr const char *applicationsOffice = "applications-office";
inline constexpr const char *applicationsOther = "applications-other";
inline constexpr const char *applicationsScience = "applications-science";
inline constexpr const char *applicationsSystem = "applications-system";
inline constexpr const char *applicationsUtilities = "applications-utilities";

inline constexpr const char *preferencesDesktop = "preferences-desktop";
inline constexpr const char *preferencesDesktopPeripherals = "preferences-desktop-peripherals";
inline constexpr const char *preferencesDesktopPersonal = "preferences-desktop-personal";
inline constexpr const char *preferencesOther = "preferences-other";
inline constexpr const char *preferencesSystem = "preferences-system";
inline constexpr const char *preferencesSystemNetwork = "preferences-system-network";

inline constexpr const char *systemHelp = "system-help";
} // namespace IconCategories

namespace IconDevices {
inline constexpr const char *audioCard = "audio-card";
inline constexpr const char *audioInputMicrophone = "audio-input-microphone";
inline constexpr const char *battery = "battery";
inline constexpr const char *cameraPhoto = "camera-photo";
inline constexpr const char *cameraVideo = "camera-video";
inline constexpr const char *cameraWeb = "camera-web";
inline constexpr const char *computer = "computer";
inline constexpr const char *driveHarddisk = "drive-harddisk";
inline constexpr const char *driveOptical = "drive-optical";
inline constexpr const char *driveRemovableMedia = "drive-removable-media";
inline constexpr const char *inputGaming = "input-gaming";
inline constexpr const char *inputKeyboard = "input-keyboard";
inline constexpr const char *inputMouse = "input-mouse";
inline constexpr const char *inputTablet = "input-tablet";
inline constexpr const char *mediaFlash = "media-flash";
inline constexpr const char *scanner = "scanner";
inline constexpr const char *videoDisplay = "video-display";
} // namespace IconDevices

namespace IconEmblems {
inline constexpr const char *emblemDefault = "emblem-default";
inline constexpr const char *emblemDocuments = "emblem-documents";
inline constexpr const char *emblemDownloads = "emblem-downloads";
inline constexpr const char *emblemFavorite = "emblem-favorite";
inline constexpr const char *emblemImportant = "emblem-important";
inline constexpr const char *emblemMail = "emblem-mail";
inline constexpr const char *emblemPhotos = "emblem-photos";
inline constexpr const char *emblemReadonly = "emblem-readonly";
inline constexpr const char *emblemShared = "emblem-shared";
inline constexpr const char *emblemSymbolicLink = "emblem-symbolic-link";
inline constexpr const char *emblemSynchronized = "emblem-synchronized";
inline constexpr const char *emblemSystem = "emblem-system";
inline constexpr const char *emblemUnreadable = "emblem-unreadable";
} // namespace IconEmblems

namespace IconMimeTypes {
inline constexpr const char *applicationXExecutable = "application-x-executable";
inline constexpr const char *applicationXFirmware = "application-x-firmware";
inline constexpr const char *applicationXSharedlib = "application-x-sharedlib";
inline constexpr const char *applicationXAddon = "application-x-addon";

inline constexpr const char *applicationXArchive = "application-x-archive";
inline constexpr const char *applicationXCompressed = "application-x-compressed";
inline constexpr const char *packageXGeneric = "package-x-generic";

inline constexpr const char *textHtml = "text-html";
inline constexpr const char *textXGeneric = "text-x-generic";
inline constexpr const char *textXGenericTemplate = "text-x-generic-template";
inline constexpr const char *textXScript = "text-x-script";

inline constexpr const char *imageXGeneric = "image-x-generic";

inline constexpr const char *audioXGeneric = "audio-x-generic";
inline constexpr const char *videoXGeneric = "video-x-generic";

inline constexpr const char *fontXGeneric = "font-x-generic";

inline constexpr const char *xOfficeAddressBook = "x-office-address-book";
inline constexpr const char *xOfficeCalendar = "x-office-calendar";
inline constexpr const char *xOfficeDocument = "x-office-document";
inline constexpr const char *xOfficePresentation = "x-office-presentation";
inline constexpr const char *xOfficeSpreadsheet = "x-office-spreadsheet";

inline constexpr const char *applicationPdf = "application-pdf";
inline constexpr const char *applicationPostscript = "application-postscript";

inline constexpr const char *applicationXObject = "application-x-object";
inline constexpr const char *applicationXTrash = "application-x-trash";

inline constexpr const char *inodeDirectory = "inode-directory";
} // namespace IconMimeTypes

} // namespace XDG
