/* OSCAR Main
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QTranslator>
#include <QSettings>
#include <QFileDialog>
#include <QFontDatabase>
#include <QStandardPaths>

#include "version.h"
#include "logger.h"
#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "translation.h"

// Gah! I must add the real darn plugin system one day.
#include "SleepLib/loader_plugins/prs1_loader.h"
#include "SleepLib/loader_plugins/cms50_loader.h"
#include "SleepLib/loader_plugins/cms50f37_loader.h"
#include "SleepLib/loader_plugins/md300w1_loader.h"
#include "SleepLib/loader_plugins/zeo_loader.h"
#include "SleepLib/loader_plugins/somnopose_loader.h"
#include "SleepLib/loader_plugins/resmed_loader.h"
#include "SleepLib/loader_plugins/intellipap_loader.h"
#include "SleepLib/loader_plugins/icon_loader.h"
#include "SleepLib/loader_plugins/weinmann_loader.h"


#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

MainWindow *mainwin = nullptr;

int compareVersion(QString version);

bool copyRecursively(QString sourceFolder, QString destFolder)
    {
        bool success = false;
        QDir sourceDir(sourceFolder);

        if(!sourceDir.exists())
            return false;

        QDir destDir(destFolder);
        if(!destDir.exists())
            destDir.mkdir(destFolder);

        QStringList files = sourceDir.entryList(QDir::Files);
        for(int i = 0; i< files.count(); i++) {
            QString srcName = sourceFolder + QDir::separator() + files[i];
            QString destName = destFolder + QDir::separator() + files[i];
            success = QFile::copy(srcName, destName);
            if(!success)
                return false;
        }

        files.clear();
        files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for(int i = 0; i< files.count(); i++)
        {
            QString srcName = sourceFolder + QDir::separator() + files[i];
            QString destName = destFolder + QDir::separator() + files[i];
//          qDebug() << "Copy from "+srcName+" to "+destName;
            success = copyRecursively(srcName, destName);
            if(!success)
                return false;
        }

        return true;
    }

bool processPreferenceFile( QString path ) {
    bool success = true;
    QString fullpath = path + "/Preferences.xml";
    qDebug() << "Process " + fullpath;
    QFile fl(fullpath);
    QFile tmp(fullpath+".tmp");
    QString line;
    fl.open(QIODevice::ReadOnly);
    tmp.open(QIODevice::WriteOnly);
    QTextStream instr(&fl);
    QTextStream outstr(&tmp);
    while (instr.readLineInto(&line)) {
        line.replace("SleepyHead","OSCAR");
        if (line.contains("VersionString")) {
            int rtAngle = line.indexOf(">", 0);
            int lfAngle = line.indexOf("<", rtAngle);
            line.replace(rtAngle+1, lfAngle-rtAngle-1, "1.0.0-beta");
        }
        outstr << line;
    }
    fl.remove();
    success = tmp.rename(fullpath);

    return success;
}

bool processFile( QString fullpath ) {
    bool success = true;
    qDebug() << "Process " + fullpath ;
    QFile fl(fullpath);
    QFile tmp(fullpath+".tmp");
    QString line;
    fl.open(QIODevice::ReadOnly);
    tmp.open(QIODevice::WriteOnly);
    QTextStream instr(&fl);
    QTextStream outstr(&tmp);
    while (instr.readLineInto(&line)) {
        line.replace("SleepyHead","OSCAR");
        outstr << line;
    }
    fl.remove();
    success = tmp.rename(fullpath);

    return success;
}

bool process_a_Profile( QString path ) {
    bool success = true;
    qDebug() << "Entering profile directory " + path;
    QDir dir(path);
    QStringList files = dir.entryList(QStringList("*.xml"), QDir::Files);
    for ( int i = 0; success && (i<files.count()); i++) {
        success = processFile( path + "/" + files[i] );
    }
    return success;
}

bool migrateFromSH(QString destDir) {
    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
    bool success = false;
    if (destDir.isEmpty()) {
        qDebug() << "Migration path is empty string";
        return success;
    }

    QString datadir = QFileDialog::getExistingDirectory(nullptr,
                      QObject::tr("Choose the SleepyHead data folder to migrate"), homeDocs, QFileDialog::ShowDirsOnly);
    qDebug() << "Migration folder selected: " + datadir;
    if (datadir.isEmpty()) {
        qDebug() << "No migration source directory selected";
        return false;
    }

    success = copyRecursively(datadir, destDir);
    if (success) {
        qDebug() << "Finished copying " + datadir;
    }

    success = processPreferenceFile( destDir );

    QDir profDir(destDir+"/Profiles");
    QStringList names = profDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (int i = 0; success && (i < names.count()); i++) {
        success = process_a_Profile( destDir+"/Profiles/"+names[i] );
    }

    return success;
}

int main(int argc, char *argv[])
{
#ifdef Q_WS_X11
    XInitThreads();
#endif

    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
    QCoreApplication::setApplicationName(getAppName());
    QCoreApplication::setOrganizationName(getDeveloperName());

    QSettings settings;
    GFXEngine gfxEngine = (GFXEngine)qMin((unsigned int)settings.value(GFXEngineSetting, (unsigned int)GFX_OpenGL).toUInt(), (unsigned int)MaxGFXEngine);

    switch (gfxEngine) {
    case 0:
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        break;
    case 1:
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        break;
    case 2:
    default:
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    }

    QString lastlanguage = settings.value(LangSetting, "").toString();

    bool dont_load_profile = false;
    bool force_data_dir = false;
    bool changing_language = false;
    QString load_profile = "";

    QApplication a(argc, argv);
    QStringList args = a.arguments();


    if (lastlanguage.isEmpty())
        changing_language = true;

    for (int i = 1; i < args.size(); i++) {
        if (args[i] == "-l")
            dont_load_profile = true;
//        else if (args[i] == "-d")
//            force_data_dir = true;
        else if (args[i] == "--language") {
            changing_language = true; // reset to force language dialog
            settings.setValue(LangSetting,"");
        } else if (args[i] == "-p")
            QThread::msleep(1000);
        else if (args[i] == "--profile") {
            if ((i+1) < args.size())
                load_profile = args[++i];
            else {
                fprintf(stderr, "Missing argument to --profile\n");
                exit(1);
            }
        } else if (args[i] == "--datadir") { // mltam's idea
            QString datadir ;
            if ((i+1) < args.size()) {
              datadir = args[++i];
              settings.setValue("Settings/AppData", homeDocs+datadir);
//            force_data_dir = true;
            } else {
              fprintf(stderr, "Missing argument to --datadir\n");
              exit(1);
            }
          }
    }   // end of for args loop


    initializeLogger();
    QThread::msleep(50); // Logger takes a little bit to catch up
#ifdef QT_DEBUG
    QString relinfo = " debug";
#else
    QString relinfo = "";
#endif
    relinfo = "("+QSysInfo::kernelType()+" "+QSysInfo::currentCpuArchitecture()+relinfo+")";
    qDebug() << STR_AppName.toLocal8Bit().data() << VersionString.toLocal8Bit().data() << relinfo.toLocal8Bit().data() << "built with Qt" << QT_VERSION_STR << "on" << __DATE__ << __TIME__;

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Language Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
    initTranslations();

    initializeStrings(); // This must be called AFTER translator is installed, but before mainwindow is setup

//    QFontDatabase::addApplicationFont("://fonts/FreeSans.ttf");
//    a.setFont(QFont("FreeSans", 11, QFont::Normal, false));

    mainwin = new MainWindow;

    ////////////////////////////////////////////////////////////////////////////////////////////
    // OpenGL Detection
    ////////////////////////////////////////////////////////////////////////////////////////////
    getOpenGLVersion();
    getOpenGLVersionString();

    //bool opengl2supported = glversion >= 2.0;
    //bool bad_graphics = !opengl2supported;
    //bool intel_graphics = false;
//#ifndef NO_OPENGL_BUILD

//#endif

/*************************************************************************************
#ifdef BROKEN_OPENGL_BUILD
    Q_UNUSED(bad_graphics)
    Q_UNUSED(intel_graphics)

    const QString BetterBuild = "Settings/BetterBuild";

    if (opengl2supported) {
        if (!settings.value(BetterBuild, false).toBool()) {
            QMessageBox::information(nullptr, QObject::tr("A faster build of OSCAR may be available"),
                QObject::tr("This build of OSCAR is a compatability version that also works on computers lacking OpenGL 2.0 support.")+"<br/><br/>"+
                QObject::tr("However it looks like your computer has full support for OpenGL 2.0!") + "<br/><br/>"+
                QObject::tr("This version will run fine, but a \"<b>%1</b>\" tagged build of OSCAR will likely run a bit faster on your computer.").arg("-OpenGL")+"<br/><br/>"+
                QObject::tr("You will not be bothered with this message again."), QMessageBox::Ok, QMessageBox::Ok);
            settings.setValue(BetterBuild, true);
        }
    }
#else
    if (bad_graphics) {
        QMessageBox::warning(nullptr, QObject::tr("Incompatible Graphics Hardware"),
            QObject::tr("This build of OSCAR requires OpenGL 2.0 support to function correctly, and unfortunately your computer lacks this capability.") + "<br/><br/>"+
            QObject::tr("You may need to update your computers graphics drivers from the GPU makers website. %1").
                arg(intel_graphics ? QObject::tr("(<a href='http://intel.com/support'>Intel's support site</a>)") : "")+"<br/><br/>"+
            QObject::tr("Because graphs will not render correctly, and it may cause crashes, this build will now exit.")+"<br/><br/>"+
            QObject::tr("There is another build available tagged \"<b>-BrokenGL</b>\" that should work on your computer."),
            QMessageBox::Ok, QMessageBox::Ok);
        exit(1);
    }
#endif
****************************************************************************************************************/
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Datafolder location Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
//  bool change_data_dir = force_data_dir;
//
//  bool havefolder = false;

    if (!settings.contains("Settings/AppData")) {       // This is first time execution
        if ( settings.contains("Settings/AppRoot") ) {  // allow for old AppRoot here - not really first time
            settings.setValue("Settings/AppData", settings.value("Settings/AppRoot"));
        } else {
            settings.setValue("Settings/AppData", getModifiedAppData());    // set up new data directory path
        }
    }

    QDir dir(GetAppData());

    if ( ! dir.exists() ) {             // directory doesn't exist, verify user's choice
        if ( ! force_data_dir ) {       // unless they explicitly selected it by --datadir param
            if (QMessageBox::question(nullptr, STR_MessageBox_Question,
                    QObject::tr("Would you like OSCAR to use this location for storing its data?")+"\n\n"+
                    QDir::toNativeSeparators(GetAppData())+"\n\n"+
                    QObject::tr("If you are upgrading, don't panic, your old data will be migrated later.")+"\n\n"+
                    QObject::tr("(If you are unsure, just click yes.)"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No) {
            // User wants a different folder for data
                bool change_data_dir = true;
                while (change_data_dir) {           // Create or select an acceptable folder
                    QString datadir = QFileDialog::getExistingDirectory(nullptr,
                                      QObject::tr("Choose or create a new folder for OSCAR data"), homeDocs, QFileDialog::ShowDirsOnly);

                    if (datadir.isEmpty()) {        // User hit Cancel instead of selecting or creating a folder
                        QMessageBox::information(nullptr, QObject::tr("Exiting"),
                            QObject::tr("As you did not select a data folder, OSCAR will exit.")+"\n\n"+QObject::tr("Next time you run, you will be asked again."));
                        return 0;
                    } else {                        // We have a folder, see if is already an OSCAR folder
                        QDir dir(datadir);
                        QFile file(datadir + "/Preferences.xml");

                        if (!file.exists()) {       // It doesn't have a Preferences.xml file in it
                            if (dir.count() > 2) {  // but it has more than dot and dotdot
                                // Not a new directory.. nag the user.
                                if (QMessageBox::question(nullptr, STR_MessageBox_Warning,
                                        QObject::tr("The folder you chose is not empty, nor does it already contain valid OSCAR data.") +
                                        "\n\n"+QObject::tr("Are you sure you want to use this folder?")+"\n\n" +
                                        datadir, QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
                                    continue;   // Nope, don't use it, go around the loop again
                                }
                            }
                        }
        
                        settings.setValue("Settings/AppData", datadir);
                        qDebug() << "Changing data folder to" << datadir;
                        change_data_dir = false;
                    }
                }           // the while loop
            }           // user wants a different folder
        }           // user used --datadir folder to select a folder
    }           // The folder doesn't exist
    qDebug() << "Using " + GetAppData() + " as OSCAR data folder";

    QDir newDir(GetAppData());
    if ( ! newDir.exists() ) {                      // directoy doesn't exist yet, try to migrate old data
        migrateFromSH( GetAppData() );              // doesn't matter if no migration
    }


    ///////////////////////////////////////////////////////////////////////////////////////////
    // Initialize preferences system (Don't use p_pref before this point)
    ///////////////////////////////////////////////////////////////////////////////////////////
    p_pref = new Preferences("Preferences");
    p_pref->Open();
    AppSetting = new AppWideSetting(p_pref);

    QString language = settings.value(LangSetting, "").toString();
    AppSetting->setLanguage(language);

//    Clean up some legacy crap
//    QFile lf(p_pref->Get("{home}/Layout.xml"));
//    if (lf.exists()) {
//        lf.remove();
//    }

    p_pref->Erase(STR_AppName);
    p_pref->Erase(STR_GEN_SkipLogin);

#ifndef NO_UPDATER
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Check when last checked for updates..
    ////////////////////////////////////////////////////////////////////////////////////////////
    QDateTime lastchecked, today = QDateTime::currentDateTime();

    bool check_updates = false;

    if (AppSetting->updatesAutoCheck()) {
        int update_frequency = AppSetting->updateCheckFrequency();
        int days = 1000;
        lastchecked = AppSetting->updatesLastChecked();

        if (lastchecked.isValid()) {
            days = lastchecked.secsTo(today);
            days /= 86400;
        }

        if (days > update_frequency) {
            check_updates = true;
        }
    }
#endif

    int vc = compareVersion(AppSetting->versionString());
    if (vc < 0) {
        AppSetting->setShowAboutDialog(1);
//      release_notes();
//      check_updates = false;
    } else if (vc > 0) {
        if (QMessageBox::warning(nullptr, STR_MessageBox_Error,
            QObject::tr("The version of OSCAR you just ran is OLDER than the one used to create this data (%1).").
                        arg(AppSetting->versionString()) +"\n\n"+
            QObject::tr("It is likely that doing this will cause data corruption, are you sure you want to do this?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {

            return 0;
        }
    }

    AppSetting->setVersionString(VersionString);

//    int id=QFontDatabase::addApplicationFont(":/fonts/FreeSans.ttf");
//    QFontDatabase fdb;
//    QStringList ffam=fdb.families();
//    for (QStringList::iterator i=ffam.begin();i!=ffam.end();i++) {
//        qDebug() << "Loaded Font: " << (*i);
//    }


    if (!p_pref->contains("Fonts_Application_Name")) {
#ifdef Q_OS_WIN
        // Windows default Sans Serif interpretation sucks
        // Segoe UI is better, but that requires OS/font detection
        (*p_pref)["Fonts_Application_Name"] = "Arial";
#else
        (*p_pref)["Fonts_Application_Name"] = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#endif
        (*p_pref)["Fonts_Application_Size"] = 10;
        (*p_pref)["Fonts_Application_Bold"] = false;
        (*p_pref)["Fonts_Application_Italic"] = false;
    }


    QApplication::setFont(QFont((*p_pref)["Fonts_Application_Name"].toString(),
                                (*p_pref)["Fonts_Application_Size"].toInt(),
                                (*p_pref)["Fonts_Application_Bold"].toBool() ? QFont::Bold : QFont::Normal,
                                (*p_pref)["Fonts_Application_Italic"].toBool()));

    qDebug() << "Selected Font" << QApplication::font().family();

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Register Importer Modules for autoscanner
    ////////////////////////////////////////////////////////////////////////////////////////////
    schema::init();
    PRS1Loader::Register();
    ResmedLoader::Register();
    IntellipapLoader::Register();
    FPIconLoader::Register();
    WeinmannLoader::Register();
    CMS50Loader::Register();
    CMS50F37Loader::Register();
    MD300W1Loader::Register();

    schema::setOrders(); // could be called in init...

    // Scan for user profiles
    Profiles::Scan();

    Q_UNUSED(changing_language)
    Q_UNUSED(dont_load_profile)

#ifndef NO_UPDATER
    if (check_updates) { mainwin->CheckForUpdates(); }
#endif

    mainwin->SetupGUI();
    mainwin->show();

    return a.exec();
}
