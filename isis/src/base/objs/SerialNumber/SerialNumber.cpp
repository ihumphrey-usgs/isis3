#include "SerialNumber.h"

#include <map>

#include <QMutex>
#include <QMutexLocker>

#include "ObservationNumber.h"
#include "SerialNumberList.h"
#include "IException.h"
#include "Pvl.h"
#include "Cube.h"
#include "Process.h"
#include "PvlToPvlTranslationManager.h"
#include "FileName.h"

namespace Isis {
  QMutex SerialNumber::m_mutex;

  /**
  * Create an empty SerialNumber object.
  */
  SerialNumber::SerialNumber() {}

  /**
   * Destroy a SerialNumber object.
  */
  SerialNumber::~SerialNumber() {}

  /**
   * Compose a SerialNumber from a PVL.
   *
   * @param label A pvl formatted label to be used to generate the serial number
   * @param def2filename If a serial number could not be found, try to return the filename
   *
   * @return Calculated SerialNumber or FileName
  */
  QString SerialNumber::Compose(Pvl &label, bool def2filename) {

    QString sn;
    try {
      PvlGroup snGroup = FindSerialTranslation(label);
      sn = CreateSerialNumber(snGroup, (int)snGroup["ObservationKeys"]);
    }
    catch(IException &) {
      if(def2filename) {
        //  Try to return the filename if it exists in the label, otherwise use
        //  "Unknown" as a last resort.
        QString snTemp = label.fileName();
        if(!snTemp.isEmpty()) {
          sn = FileName(snTemp).name();
        }
        else {
          sn = "Unknown";
        }
      }
      else {
        sn = "Unknown";
      }
    }

    return sn;
  }

  /**
   * Compose a SerialNumber from a Cube.
   *
   * @param cube An opened Isis cub
   * @param def2filename If a serial number could not be found, try to return the filename
   *
   * @return Calculated SerialNumber or FileName
  */
  QString SerialNumber::Compose(Cube &cube, bool def2filename) {
    return Compose(*cube.label(), def2filename);
  }

  /**
   * Compose a SerialNumber from a file.
   *
   * @param filename a filename to open
   * @param def2filename If a serial number could not be found, try to return the
   *                     filename
   *
   * @return Calculated SerialNumber or FileName
  */
  QString SerialNumber::Compose(const QString &filename, bool def2filename) {
    Pvl p(filename);
    return Compose(p, def2filename);
  }

  /**
   * Get Groups by translating from correct Translation table
   *
   * This method is thread-safe.
   *
   * @param label A pvl formatted label to be used to generate the serial number
   *
   */
  PvlGroup SerialNumber::FindSerialTranslation(Pvl &label) {
    // Immediately lock the static mutex so multiple threads don't collide on the static variables
    // inside of this method. This locker auto-locks upon construction and unlocks the mutex when
    // it is destroyed (when this method finishes).
    QMutexLocker lock(&m_mutex);

    Pvl outLabel;
    static PvlGroup dataDir(Preference::Preferences().findGroup("DataDirectory"));

    // Get the mission name
    static QString missionTransFile = (QString) dataDir["base"] + "/translations/MissionName2DataDir.trn";
    static PvlToPvlTranslationManager missionXlater(missionTransFile);
    missionXlater.SetLabel(label);
    QString mission = missionXlater.Translate("MissionName");

    // Get the instrument name
    static QString instTransFile = (QString) dataDir["base"] + "/translations/Instruments.trn";
    static PvlToPvlTranslationManager instrumentXlater(instTransFile);
    instrumentXlater.SetLabel(label);
    QString instrument = instrumentXlater.Translate("InstrumentName");

    // We want to use this instrument's translation manager. It's much faster for
    //   SerialNumberList if we keep the translation manager in memory, so re-reading
    //   from the disk is not necessary every time. To do this, we'll use a map to store
    //   the translation managers with a string identifier to find them. This identifier
    //   needs to have the mission name and the instrument name.

    //  Create the static map to keep the translation managers in memory
    static std::map<QString, PvlToPvlTranslationManager> missionTranslators;

    // Determine the key for this translation manager - must have both mission and instrument
    QString key = mission + "_" + instrument;

    // Try to find an existing translation manager with the key
    std::map<QString, PvlToPvlTranslationManager>::iterator translationIterator = missionTranslators.find(key);

    // If we don't succeed, create one
    if(translationIterator == missionTranslators.end()) {
      // Get the file
      FileName snFile((QString) dataDir[mission] + "/translations/" +
                                    instrument + "SerialNumber????.trn");
      snFile = snFile.highestVersion();

      // use the translation file to generate keywords
      missionTranslators.insert(
        std::pair<QString, PvlToPvlTranslationManager>(key, PvlToPvlTranslationManager(snFile.expanded()))
      );

      translationIterator = missionTranslators.find(key);
    }

    translationIterator->second.SetLabel(label);
    translationIterator->second.Auto(outLabel);

    PvlGroup snGroup = outLabel.findGroup("SerialNumberKeywords");
    snGroup += PvlKeyword("ObservationKeys", toString(snGroup.keywords()));

    return snGroup;
  }

  /**
   * Create the SerialNumber string by concatenating the keywords in the label
   * with '/' in between serialNumber groups and the number of observationKeys
   *
   * @param snGroup A PvlGroup containing the keywords to concatenate
   * @param keys the number of strings to contatenate
   */
  QString SerialNumber::CreateSerialNumber(PvlGroup &snGroup, int keys) {
    QString sn = snGroup["Keyword1"][0];
    for(int i = 2; i <= keys; i++) {
      QString keyword = QString("Keyword%1").arg(i);
      sn += "/" + snGroup[keyword][0];
    }
    return sn;
  }

  /**
   * Creates the ObservationNumber from a string representing the
   * SerialNumber and a SerialList.
   *
   * @param sn the string representing the SerialNumber
   *
   * @param list the SerialNumberList
   * @param def2filename If a serial number could not be found, try to return the
   *                     filename
   *
   * @return Calculated SerialNumber or FileName
  */
  QString SerialNumber::ComposeObservation(const QString &sn, SerialNumberList &list, bool def2filename) {
    QString filename = list.fileName(sn);
    return ObservationNumber::Compose(filename, def2filename);
  }
}
