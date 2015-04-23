#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/forms/merge/tab.h"
#include "mkvtoolnix-gui/merge/tab.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

namespace mtx { namespace gui { namespace Merge {

using namespace mtx::gui;

void
Tab::setupOutputControls() {
  m_splitControls << ui->splitOptions << ui->splitOptionsLabel << ui->splitMaxFilesLabel << ui->splitMaxFiles << ui->linkFiles;

  auto comboBoxControls = QList<QComboBox *>{} << ui->splitMode << ui->chapterLanguage << ui->chapterCharacterSet;
  for (auto const &control : comboBoxControls)
    control->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  onSplitModeChanged(MuxConfig::DoNotSplit);
}

void
Tab::onTitleEdited(QString newValue) {
  m_config.m_title = newValue;
}

void
Tab::setDestination(QString const &newValue) {
  m_config.m_destination = newValue;
  emit titleChanged();
}

void
Tab::onBrowseOutput() {
  auto filter   = m_config.m_webmMode ? QY("WebM files") + Q(" (*.webm)") : QY("Matroska files") + Q(" (*.mkv *.mka *.mks *.mk3d)");
  auto fileName = getSaveFileName(QY("Select output file name"), filter, ui->output);
  if (fileName.isEmpty())
    return;

  setDestination(fileName);

  auto &settings           = Util::Settings::get();
  settings.m_lastOutputDir = QFileInfo{ fileName }.absoluteDir();
  settings.save();
}

void
Tab::onGlobalTagsEdited(QString newValue) {
  m_config.m_globalTags = newValue;
}

void
Tab::onBrowseGlobalTags() {
  auto fileName = getOpenFileName(QY("Select tags file"), QY("XML tag files") + Q(" (*.xml)"), ui->globalTags);
  if (!fileName.isEmpty())
    m_config.m_globalTags = fileName;
}

void
Tab::onSegmentInfoEdited(QString newValue) {
  m_config.m_segmentInfo = newValue;
}

void
Tab::onBrowseSegmentInfo() {
  auto fileName = getOpenFileName(QY("Select segment info file"), QY("XML segment info files") + Q(" (*.xml)"), ui->segmentInfo);
  if (!fileName.isEmpty())
    m_config.m_segmentInfo = fileName;
}

void
Tab::onSplitModeChanged(int newMode) {
  auto splitMode       = static_cast<MuxConfig::SplitMode>(newMode);
  m_config.m_splitMode = splitMode;

  if (MuxConfig::DoNotSplit == splitMode) {
    Util::enableWidgets(m_splitControls, false);
    return;
  }

  Util::enableWidgets(m_splitControls, true);

  auto tooltip = QStringList{};
  auto entries = QStringList{};
  auto label   = QString{};

  if (MuxConfig::SplitAfterSize == splitMode) {
    label    = QY("Size:");
    tooltip << QY("The size after which a new output file is started.")
            << QY("The letters 'G', 'M' and 'K' can be used to indicate giga/mega/kilo bytes respectively.")
            << QY("All units are based on 1024 (G = 1024^3, M = 1024^2, K = 1024).");
    entries << Q("")
            << Q("350M")
            << Q("650M")
            << Q("700M")
            << Q("703M")
            << Q("800M")
            << Q("1000M")
            << Q("4483M")
            << Q("8142M");

  } else if (MuxConfig::SplitAfterDuration == splitMode) {
    label    = QY("Duration:");
    tooltip << QY("The duration after which a new output file is started.")
            << (Q("%1 %2 %3")
                .arg(QY("The format is either the form 'HH:MM:SS.nnnnnnnnn' or a number followed by one of the units 's', 'ms' or 'us'."))
                .arg(QY("You may omit the number of hours 'HH' and the number of nanoseconds 'nnnnnnnnn'."))
                .arg(QY("If given then you may use up to nine digits after the decimal point.")))
            << QY("Examples: 01:00:00 (after one hour) or 1800s (after 1800 seconds).");

    entries << Q("")
            << Q("01:00:00")
            << Q("1800s");

  } else if (MuxConfig::SplitAfterTimecodes == splitMode) {
    label    = QY("Timecodes:");
    tooltip << (Q("%1 %2")
                .arg(QY("The timecodes after which a new output file is started."))
                .arg(QY("The timecodes refer to the whole stream and not to each individual output file.")))
            << (Q("%1 %2 %3")
                .arg(QY("The format is either the form 'HH:MM:SS.nnnnnnnnn' or a number followed by one of the units 's', 'ms' or 'us'."))
                .arg(QY("You may omit the number of hours 'HH'."))
                .arg(QY("You can specify up to nine digits for the number of nanoseconds 'nnnnnnnnn' or none at all.")))
            << (Q("%1 %2")
                .arg(QY("If two or more timecodes are used then you have to separate them with commas."))
                .arg(QY("The formats can be mixed, too.")))
            << QY("Examples: 01:00:00,01:30:00 (after one hour and after one hour and thirty minutes) or 180s,300s,00:10:00 (after three, five and ten minutes).");

  } else if (MuxConfig::SplitByParts == splitMode) {
    label    = QY("Parts:");
    tooltip << QY("A comma-separated list of timecode ranges of content to keep.")
            << (Q("%1 %2")
                .arg(QY("Each range consists of a start and end timecode with a '-' in the middle, e.g. '00:01:15-00:03:20'."))
                .arg(QY("If a start timecode is left out then the previous range's end timecode is used, or the start of the file if there was no previous range.")))
            << QY("The format is either the form 'HH:MM:SS.nnnnnnnnn' or a number followed by one of the units 's', 'ms' or 'us'.")
            << QY("If a range's start timecode is prefixed with '+' then its content will be written to the same file as the previous range. Otherwise a new file will be created for this range.");

  } else if (MuxConfig::SplitByPartsFrames == splitMode) {
    label    = QY("Parts:");
    tooltip << (Q("%1 %2 %3")
                .arg(QY("A comma-separated list of frame/field number ranges of content to keep."))
                .arg(QY("Each range consists of a start and end frame/field number with a '-' in the middle, e.g. '157-238'."))
                .arg(QY("The numbering starts at 1.")))
            << (Q("%1 %2")
                .arg(QY("This mode considers only the first video track that is output."))
                .arg(QY("If no video track is output no splitting will occur.")))
            << (Q("%1 %2 %3")
                .arg(QY("The numbers given with this argument are interpreted based on the number of Matroska blocks that are output."))
                .arg(QY("A single Matroska block contains either a full frame (for progressive content) or a single field (for interlaced content)."))
                .arg(QY("mkvmerge does not distinguish between those two and simply counts the number of blocks.")))
            << (Q("%1 %2")
                .arg(QY("If a start number is left out then the previous range's end number is used, or the start of the file if there was no previous range."))
                .arg(QY("If a range's start number is prefixed with '+' then its content will be written to the same file as the previous range. Otherwise a new file will be created for this range.")));

  } else if (MuxConfig::SplitByFrames == splitMode) {
    label    = QY("Frames/fields:");
    tooltip << (Q("%1 %2")
                .arg(QY("A comma-separated list of frame/field numbers after which to split."))
                .arg(QY("The numbering starts at 1.")))
            << (Q("%1 %2")
                .arg(QY("This mode considers only the first video track that is output."))
                .arg(QY("If no video track is output no splitting will occur.")))
            << (Q("%1 %2 %3")
                .arg(QY("The numbers given with this argument are interpreted based on the number of Matroska blocks that are output."))
                .arg(QY("A single Matroska block contains either a full frame (for progressive content) or a single field (for interlaced content)."))
                .arg(QY("mkvmerge does not distinguish between those two and simply counts the number of blocks.")));

  } else if (MuxConfig::SplitAfterChapters == splitMode) {
    label    = QY("Chapter numbers:");
    tooltip << (Q("%1 %2")
                .arg(QY("Either the word 'all' which selects all chapters or a comma-separated list of chapter numbers before which to split."))
                .arg(QY("The numbering starts at 1.")))
            << QY("Splitting will occur right before the first key frame whose timecode is equal to or bigger than the start timecode for the chapters whose numbers are listed.")
            << (Q("%1 %2")
                .arg(QY("A chapter starting at 0s is never considered for splitting and discarded silently."))
                .arg(QY("This mode only considers the top-most level of chapters across all edition entries.")));

  }

  auto options = ui->splitOptions->currentText();

  ui->splitOptionsLabel->setText(label);
  ui->splitOptions->clear();
  ui->splitOptions->addItems(entries);
  ui->splitOptions->setCurrentText(options);

  Util::setToolTip(ui->splitOptions, Q("<p>%1</p>").arg(tooltip.join(Q("</p><p>"))));
}

void
Tab::onSplitOptionsEdited(QString newValue) {
  m_config.m_splitOptions = newValue;
}

void
Tab::onLinkFilesClicked(bool newValue) {
  m_config.m_linkFiles = newValue;
}

void
Tab::onSplitMaxFilesChanged(int newValue) {
  m_config.m_splitMaxFiles = newValue;
}

void
Tab::onSegmentUIDsEdited(QString newValue) {
  m_config.m_segmentUIDs = newValue;
}

void
Tab::onPreviousSegmentUIDEdited(QString newValue) {
  m_config.m_previousSegmentUID = newValue;
}

void
Tab::onNextSegmentUIDEdited(QString newValue) {
  m_config.m_nextSegmentUID = newValue;
}

void
Tab::onChaptersEdited(QString newValue) {
  m_config.m_chapters = newValue;
}

void
Tab::onBrowseChapters() {
  auto fileName = getOpenFileName(QY("Select chapter file"), QY("XML chapter files") + Q(" (*.xml)"), ui->chapters);
  if (!fileName.isEmpty())
    m_config.m_chapters = fileName;
}

void
Tab::onChapterLanguageChanged(int newValue) {
  auto data = ui->chapterLanguage->itemData(newValue);
  if (data.isValid())
    m_config.m_chapterLanguage = data.toString();
}

void
Tab::onChapterCharacterSetChanged(QString newValue) {
  m_config.m_chapterCharacterSet = newValue;
}

void
Tab::onChapterCueNameFormatChanged(QString newValue) {
  m_config.m_chapterCueNameFormat = newValue;
}

void
Tab::onWebmClicked(bool newValue) {
  m_config.m_webmMode = newValue;
  // TODO: change output file extension
}

void
Tab::onUserDefinedOptionsEdited(QString newValue) {
  m_config.m_userDefinedOptions = newValue;
}

void
Tab::onEditUserDefinedOptions() {
  // TODO
}

void
Tab::setOutputControlValues() {
  ui->title->setText(m_config.m_title);
  ui->output->setText(m_config.m_destination);
  ui->globalTags->setText(m_config.m_globalTags);
  ui->segmentInfo->setText(m_config.m_segmentInfo);
  ui->splitMode->setCurrentIndex(m_config.m_splitMode);
  ui->splitOptions->setEditText(m_config.m_splitOptions);
  ui->splitMaxFiles->setValue(m_config.m_splitMaxFiles);
  ui->linkFiles->setChecked(m_config.m_linkFiles);
  ui->segmentUIDs->setText(m_config.m_segmentUIDs);
  ui->previousSegmentUID->setText(m_config.m_previousSegmentUID);
  ui->nextSegmentUID->setText(m_config.m_nextSegmentUID);
  ui->chapters->setText(m_config.m_chapters);
  ui->chapterCueNameFormat->setText(m_config.m_chapterCueNameFormat);
  ui->userDefinedOptions->setText(m_config.m_userDefinedOptions);
  ui->webmMode->setChecked(m_config.m_webmMode);

  Util::setComboBoxTextByData(ui->chapterLanguage,     m_config.m_chapterLanguage);
  Util::setComboBoxTextByData(ui->chapterCharacterSet, m_config.m_chapterCharacterSet);
}

}}}
