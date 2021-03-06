#include "updatedialog.h"
#include "ui_updatedialog.h"

#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <QDebug>

static const char defaultFileName[] = "qtvt_release.html";

updatedialog::updatedialog(QWidget *parent, OSInfo osinfo, bool autoclose):
    QDialog(parent),
    ui(new Ui::updatedialog)
{
    ui->setupUi(this);
    cpuArch = osinfo.cpuArch;
    productType = osinfo.productType;
    m_autoclose = autoclose;

    ui->currentVersion->setText(APP_VERSION);
    ui->latestVersion->setText("checking");
    connect(ui->updateButton, SIGNAL(pressed()), this, SLOT(getUpdate()));

    //tmp download Directory
    downloadDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (downloadDirectory.isEmpty() || !QFileInfo(downloadDirectory).isDir())
        downloadDirectory = QDir::currentPath();

    ui->AutoCloseCheckBox->setChecked(m_autoclose);
    connect(ui->AutoCloseCheckBox,SIGNAL(stateChanged(int)), this, SLOT(setAutoCloseTimer(int)));
    //ui->AutoCloseLabel->setVisible(false);

    autoclosecount = 10;
    autoclosetimer= new QTimer(this);
    autoclosetimer->setInterval(1000);//1 sec
    connect(autoclosetimer, SIGNAL(timeout()), this, SLOT(timerHandler()));
    //autoclosetimer.setSingleShot(true);

    downloadFile();
}

updatedialog::~updatedialog()
{
    delete ui;
}
void updatedialog::timerHandler()
{
    autoclosecount--;
    QString s = QString("Close (%1)").arg(QString::number(autoclosecount));
    setCloseCaption(s);
    if (autoclosecount<=0) {
        autoclosecount=10;
        autoclosetimer->stop();
        setCloseCaption("Close");
        close();
    }
}
void updatedialog::setCloseCaption(QString title)
{
    ui->buttonBox->button(QDialogButtonBox::Close)->setText(title);
}

void updatedialog::setAutoClose(bool close)
{
    m_autoclose = close;
}
void updatedialog::setAutoCloseTimer(int state)
{
    bool checked;
    if (state==0) {
        //no check
        if (autoclosetimer->isActive()) {
            autoclosetimer->stop();
        }
        checked = false;
        setCloseCaption("Close");
    } else {
        if (!autoclosetimer->isActive()) {
            autoclosetimer->start();
        }
        checked = true;
    }
    autoclosecount = 10;
    QSettings set;
    set.beginGroup("Main");
    set.setValue("AutoCloseUpdate", checked);
    set.endGroup();
}

void updatedialog::downloadFile()
{
    const QUrl newUrl = QUrl::fromUserInput(MYRELEASEURL);
    if (!newUrl.isValid()) {
        QMessageBox::information(this, tr("Error"),
                                 tr("Invalid URL: %1: %2").arg(MYRELEASEURL, newUrl.errorString()));
        return;
    }

    QString fileName = newUrl.fileName();
    if (fileName.isEmpty())
        fileName = defaultFileName;

    file = openFileForWrite(fileName);
    if (!file)
        return;

    // schedule the request
    startRequest(newUrl);
}

void updatedialog::startRequest(const QUrl &requestedUrl)
{
    url = requestedUrl;
    httpRequestAborted = false;

    QNetworkRequest request(url);
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    reply = qnam.get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(httpFinished()));
    connect(reply, SIGNAL(readyRead()), this, SLOT(httpReadyRead()));
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(networkReplyProgress(qint64, qint64)));

    setStatus(tr("Downloading %1...").arg(url.toString()));
}
void updatedialog::httpFinished()
{
    ui->progressBar->setValue(0);

    QFileInfo fi;
    if (file) {
        fi.setFile(file->fileName());
        file->close();

        delete file;
        file = Q_NULLPTR;
    }

    if (httpRequestAborted) {
        reply->deleteLater();
        reply = Q_NULLPTR;
        return;
    }

    if (reply->error()) {
        QFile::remove(fi.absoluteFilePath());
        setStatus(tr("Error! not found latest version."));
        networkReplyProgress(0,100);
        reply->deleteLater();
        reply = Q_NULLPTR;
        return;
    }

    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    reply->deleteLater();
    reply = Q_NULLPTR;

    if (!redirectionTarget.isNull()) {
        const QUrl redirectedUrl = url.resolved(redirectionTarget.toUrl());
        if (QMessageBox::question(this, tr("Redirect"),
                                  tr("Redirect to %1 ?").arg(redirectedUrl.toString()),
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
            return;
        }
        file = openFileForWrite(fi.absoluteFilePath());
        if (!file) {
            return;
        }
        startRequest(redirectedUrl);
        return;
    }

    //parser JSON file
    QJsonDocument jsonDoc = loadJson(fi.absoluteFilePath());

    //QString cpuArch = QSysInfo::currentCpuArchitecture();
    qDebug() << "CpuArch: " << cpuArch;
    //QString productType = QSysInfo::productType();
    qDebug() << "productType:" << productType;
    //qDebug() << "productVersion:" << QSysInfo::productVersion();

    QJsonObject json_obj=jsonDoc.object();
    QString name=json_obj["tag_name"].toString();
    ui->latestVersion->setText(name);
    if (isLatestVersionExist(name)) {
        //record Latest file to be download and install
        //assets/name, browser_download_url, size
        QJsonArray assets_arr=json_obj["assets"].toArray();
        for(int i=0; i<assets_arr.size(); i++){
            QJsonObject assets_obj=assets_arr[i].toObject();

            //qDebug() <<"assets name:" << assets_obj["name"].toString();
            //qDebug() <<"assets browser_download_url:" << assets_obj["browser_download_url"].toString();
            //qDebug() <<"assets size:" << assets_obj["size"].toInt();

            latestDLFilename = assets_obj["name"].toString();
            latestDLUrl = assets_obj["browser_download_url"].toString();
            latestDLSize = assets_obj["size"].toDouble();
            if (productType == "ubuntu") {
                QFileInfo info(latestDLFilename);
                if (info.suffix() == "deb") {
                    if (cpuArch == "x86_64") {
                        if (latestDLFilename.contains("amd64")) {
                            break;
                        }
                    } else {
                        if (latestDLFilename.contains("i386")) {
                            break;
                        }
                    }
                }
            } else if (productType == "windows") {
                QFileInfo info(latestDLFilename);
                if (info.suffix() == "exe") {
                    //Windows's setup.exe which content x86/x64 binary
                    break;
                }
            } else {
                qDebug() << "TODO: support platform:" << productType;
            }

        }
        setStatus(tr("Found New version!"));
        ui->updateButton->setEnabled(true);
    } else {
        setStatus(tr("No new version founded!"));
        if (m_autoclose) {
            autoclosetimer->start();
        }
    }
}
void updatedialog::httpReadyRead()
{
    // this slot gets called every time the QNetworkReply has new data.
    // We read all of its new data and write it into the file.
    // That way we use less RAM than when reading it at the finished()
    // signal of the QNetworkReply
    if (file) {
        file->write(reply->readAll());
    }
}

QFile *updatedialog::openFileForWrite(const QString &fileName)
{
    if (fileExists(fileName)) {
        QFile::remove(fileName);
    }

    QScopedPointer<QFile> file(new QFile(fileName));
    if (!file->open(QIODevice::ReadWrite)) {
    //if (!file->open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Error"),
                                 tr("Unable to save the file %1: %2.")
                                 .arg(QDir::toNativeSeparators(fileName),
                                      file->errorString()));
        return Q_NULLPTR;
    }
    return file.take();
}
void updatedialog::networkReplyProgress(qint64 bytesRead, qint64 totalBytes)
{
    ui->progressBar->setMaximum(totalBytes);
    ui->progressBar->setValue(bytesRead);
}
bool updatedialog::fileExists(QString path) {
    QFileInfo check_file(path);
    // check if file exists and if yes: Is it really a file and no directory?
    return check_file.exists() && check_file.isFile();
}
QJsonDocument updatedialog::loadJson(QString fileName)
{
    QFile jsonFile(fileName);
    jsonFile.open(QFile::ReadOnly);
    return QJsonDocument().fromJson(jsonFile.readAll());
}

bool updatedialog::isLatestVersionExist(QString latestVersion)
{
    QString ver = APP_VERSION;
    if (cmpVersion(latestVersion.replace("v","").toLocal8Bit().data(),
                   ver.toLocal8Bit().data()) >0) {
        return true;
    } else {
        return false;
    }
}

/*
 * return 1 if v1 > v2
 * return 0 if v1 = v2
 * return -1 if v1 < v2
 */

int updatedialog::cmpVersion(const char *v1, const char *v2)
{
    int i;
    int oct_v1[4], oct_v2[4];
    sscanf(v1, "%d.%d.%d.%d", &oct_v1[0], &oct_v1[1], &oct_v1[2], &oct_v1[3]);
    sscanf(v2, "%d.%d.%d.%d", &oct_v2[0], &oct_v2[1], &oct_v2[2], &oct_v2[3]);

    for (i = 0; i < 4; i++) {
        if (oct_v1[i] > oct_v2[i])
            return 1;
        else if (oct_v1[i] < oct_v2[i])
            return -1;
    }

    return 0;
}

void updatedialog::setStatus(QString msg)
{
    ui->statusLabel->setText(msg);
}
void updatedialog::getUpdate()
{
    qDebug() << "download latestDLUrl: " << latestDLUrl;
    QNetworkRequest request;
    request.setUrl(latestDLUrl);

    reply = qnam.get(request); // Manager is my QNetworkAccessManager

    QString dest = downloadDirectory+QDir::separator()+latestDLFilename;//.append();
    dlfile = new QFile(dest); // "des" is the file path to the destination file
    dlfile->open(QIODevice::WriteOnly);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),this, SLOT(dlError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)),  this, SLOT(dlProgress(qint64, qint64)));
    connect(reply, SIGNAL(finished()),   this, SLOT(dlFinished()));
    connect(reply, SIGNAL(readyRead()), this, SLOT(dlReadyRead()));
}
void updatedialog::dlError(QNetworkReply::NetworkError err)
{
    qDebug() <<"dlError: " << err;
    // Manage error here.
    reply->deleteLater();
    Q_UNUSED(err);
}
void updatedialog::dlProgress(qint64 read, qint64 total)
{
    ui->progressBar->setMaximum(total);
    ui->progressBar->setValue(read);
}
void updatedialog::dlReadyRead()
{
    // this slot gets called every time the QNetworkReply has new data.
    // We read all of its new data and write it into the file.
    // That way we use less RAM than when reading it at the finished()
    // signal of the QNetworkReply
    if (dlfile) {
        dlfile->write(reply->readAll());
    }
}

void updatedialog::dlFinished()
{

    //handle redirect
    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (!redirectionTarget.isNull()) {
        const QUrl redirectedUrl = url.resolved(redirectionTarget.toUrl());
        /* no need to confirm!
        if (QMessageBox::question(this, tr("Redirect"),
                                  tr("Redirect to %1 ?").arg(redirectedUrl.toString()),
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
            return;
        }*/
        dlfile->resize(0);
        latestDLUrl = redirectedUrl;
        getUpdate();
        return;
    } else {
        if (dlfile) {
            dlfile->close();
        }
        //signal the downloaded file
        emit doExec(dlfile->fileName());
        //kill self
        emit doExit();
    }
    /* Clean up. */
    reply->deleteLater();
}
