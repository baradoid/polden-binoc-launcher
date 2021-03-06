#include "web.h"

#include <QTimer>
#include <QUrlQuery>
#include <QFileInfo>
#include <QDir>
#include <QHttpMultiPart>
#include <QProcess>
#include <QDebug>
#include "utils.h"

Web::Web(QObject *parent) : QObject(parent),
    nam(this), webConnectionState(idleState)
{
    connect(&nam, SIGNAL(finished(QNetworkReply*)),
                this, SLOT(handleNamReplyFinished(QNetworkReply*)));
}

void Web::start()
{
    QTimer::singleShot(100, this, SLOT(handleWbLoginTimeout()));
}


void Web::handleNamReplyFinished(QNetworkReply* repl)
{
        QList<QNetworkReply::RawHeaderPair> headPairs = repl->rawHeaderPairs();

//        foreach(QNetworkReply::RawHeaderPair  p,  headPairs){
//            qDebug() << p.first << p.second;
//        }

        //qDebug() << readed;
        //qDebug() << uq.queryPairDelimiter() << uq.queryValueDelimiter();
        //uq.setQueryDelimiters('=', '|');
        //qDebug() << uq.queryPairDelimiter() << uq.queryValueDelimiter();
        //uq.allQueryItemValues()
        //qDebug() << "GUID: " << uq.queryItemValue("guid").toLatin1();

        QString reqStr = repl->request().url().toString();
        //qDebug() << "req: " << qPrintable(reqStr) << " repl: " << readed;
        //qDebug() << "handleNamReplyFinished" << qPrintable(repl->url().toString());
        QString reqUrl = repl->url().toString();

        if(reqUrl.endsWith("login.php")){
            processLoginReply(repl);
        }
        else if(reqUrl.endsWith("tasks.php")){
            processTasksReply(repl);
        }
        else if(reqUrl.endsWith("alive.php")){
            processAliveReply(repl);
        }
        else if(reqUrl.endsWith("upload_logs.php")){
            processUploadLogsReply(repl);

        }
        else if(reqUrl.endsWith("progress.php")){
            processProgressReply(repl);
        }
        else if(reqUrl.contains("/programs/") == true){
            if(processDownloaded(repl) == true){
                emit msg(QString("post progress:\"taskId:%1 done\"").arg(dlProgTaskId));
                postProgress(dlProgTaskId);
            }
        }
        else if(reqUrl.contains("/projects/") == true){
            if(processDownloaded(repl) == true){
                emit msg(QString("post progress:\"taskId:%1 done\"").arg(dlProjTaskId));
                postProgress(dlProjTaskId);
            }
        }


//        if(webState == idle){

//        }
//        else if(webState == connected){
//        }
}

void Web::handleWbLoginTimeout()
{
    //QString wbUser = ui->lineEdit_wbUser->text();
    //QString wbPass = ui->lineEdit_wbPass->text();

    //QString wbPath = ui->lineEdit_wbPath->text();
 //   QByteArray requestString = "login=bino&psswd=bino12345";
    QUrlQuery params;
    params.addQueryItem("login", wbUser);
    params.addQueryItem("psswd", wbPass);

    QNetworkRequest request;
    request.setUrl(QUrl(wbPath+"/login.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    nam.post(request, params.toString().toLatin1());
}

void Web::processLoginReply(QNetworkReply* repl)
{
    //qDebug() << "QNetworkReply finished";
    if(repl->error() == QNetworkReply::NoError){
        QString readed(repl->readAll());
        readed.replace('|', '&');
        QUrlQuery uq(readed);

        webConnectionState = loginSuccessState;
        guid = uq.queryItemValue("guid");
        emit msg(readed); //appendLogString();
        if(guid.isEmpty() == false){
            emit msg("connection success"); //appendLogString("WEB: connection success");
            emit connSuccess();

            QTimer::singleShot(10*1000, this, SLOT(handlePostAliveTimeout()));
            QTimer::singleShot(20*1000, this, SLOT(handlePostTasksTimeout()));

            //upload today log

            QFileInfo fi(dirStruct->logsDir + todayLogPath());
            QString todayLogZipPath = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + ".zip";
            if(zip(fi.absoluteFilePath(), todayLogZipPath) == false){
                emit msg("zip file " + fi.absoluteFilePath() + " failed");
            }
            else{
                if(webUploadTodayLogAsMultiPart(todayLogZipPath) == true)
                    emit msg("today log zip upload POST success: \"" + todayLogZipPath + "\"");
                else
                    emit msg("today log .zip upload POST failed: \"" + todayLogZipPath + "\"");
            }

            //cleanZipTemporaryFiles();

            //webUploadTodayLogAsRequest(todayLogZipPath);
        }
        else{
            emit msg("no guid returned");
            emit connError("no guid returned");
            QTimer::singleShot(10*1000, this, SLOT(handleWbLoginTimeout()));
        }
    }
    else{
        //setConnectionError(repl->errorString());
        if(webConnectionState != connectionRefusedState){
            webConnectionState = connectionRefusedState;
            emit msg("login request error:\"" + repl->errorString() +"\"");
        }
        emit connError(repl->errorString());

        QTimer::singleShot(10*1000, this, SLOT(handleWbLoginTimeout()));
    }
}

void Web::cleanZipTemporaryFiles()
{
    QDir dir(dirStruct->logsDir);
    QStringList filters;
    filters << "zia*";
    QStringList strList = dir.entryList(filters);

    foreach (QString ss, strList) {
        qDebug() << qPrintable(dir.absolutePath() + "/" + ss);
        QFile::remove(dir.absolutePath() + "/" + ss);
    }


}

void Web::handlePostAliveTimeout()
{
    //QString wbPath = ui->lineEdit_wbPath->text();
    QUrlQuery params;
    params.addQueryItem("guid", guid.toLatin1());

    QNetworkRequest request;
    request.setUrl(QUrl(wbPath+"/alive.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    nam.post(request, params.toString().toLatin1());
}

void Web::postProgress(QString taskId)
{
    QUrlQuery params;
    params.addQueryItem("guid", guid.toLatin1());
    params.addQueryItem("task_id", taskId.toLatin1());
    params.addQueryItem("progress", "100");
    params.addQueryItem("done", "1");

    QNetworkRequest request;
    request.setUrl(QUrl(wbPath+"/progress.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    QNetworkReply *nr = nam.post(request, params.toString().toLatin1());
    replyMap[nr] = taskId;
}

void Web::processAliveReply(QNetworkReply* repl)
{
    if(repl->error() == QNetworkReply::NoError){
        QString readed(repl->readAll());
        readed.replace('|', '&');
        //QUrlQuery uq(readed);

        if(readed == "ok"){
            emit connSuccess();
        }
        else{
            //qDebug() << "req: " << qPrintable(reqStr) << " repl: " << readed;
            emit msg("unexpected repl on alive req: " + readed);
            emit connError("unexpected response");
        }
    }
    else{
        emit msg("error repl on alive req: " + repl->errorString());
        emit connError(repl->errorString());
    }

    QTimer::singleShot(60*1000, this, SLOT(handlePostAliveTimeout()));
}

void Web::processProgressReply(QNetworkReply* repl)
{
    if(repl->error() == QNetworkReply::NoError){
        //qDebug() << "processProgressReply " << replyMap[repl].toLatin1();
        QString readed(repl->readAll());
        readed.replace('|', '&');
        //QUrlQuery uq(readed);

        if(readed == "ok"){
            emit msg(QString("post progress on taskId \"")+replyMap[repl]+ QString("\" success with:\"")+ readed +"\"");
            //emit connSuccess();
        }
        else{
            emit msg(QString("post progress on taskId \"")+replyMap[repl]+ QString("\" fail with:\"")+ readed +"\"");
            //qDebug() << "req: " << qPrintable(reqStr) << " repl: " << readed;
            //emit msg("unexpected repl on progress req: " + readed);
            //emit connError("unexpected response");
        }
    }
    else{
        emit msg(QString("post progress on taskId \"")+replyMap[repl]+ QString("\" fail with:\"")+ repl->errorString() +"\"");
    }

    QTimer::singleShot(60*1000, this, SLOT(handlePostAliveTimeout()));
}

void Web::processTasksReply(QNetworkReply* repl)
{
    if(repl->error() == QNetworkReply::NoError){
        QString readed(repl->readAll());
        readed.replace('|', '&');
        QUrlQuery uq(readed);

        //qDebug() << "req: " << qPrintable(reqStr) << " repl: " << readed;
        if(readed.contains("Error") == false){
            QString tasks("incoming tasks: ");
            tasks += readed;
            emit msg(tasks);
            processTasks(readed);
        }
        else{
            emit msg(QString("task request fail: \"")+readed+("\""));
        }
    }
    else{
        emit msg("error repl on tasks req: " + repl->errorString());
        emit connError(repl->errorString());
    }
    QTimer::singleShot(20*1000, this, SLOT(handlePostTasksTimeout()));
}



void Web::webUploadTodayLogAsRequest(QString todayLogPath)
{
    QFile file(todayLogPath);
    if(file.open(QIODevice::ReadOnly) == false){
        emit msg("error today log file open");
        return;
    }
    QByteArray ba = file.readAll();
    qDebug("ba: %d", ba.length());


    //QString wbPath = ui->lineEdit_wbPath->text();
    QUrlQuery params;
    params.addQueryItem("guid", guid.toLatin1());
    //params.addQueryItem();
    //Content-Type: application/octet-stream
    //Content-Disposition: attachment

    params.addQueryItem("data", ba.toHex());
    //params.addQueryItem();

    QNetworkRequest request;
    request.setUrl(QUrl(wbPath+"/upload_logs.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    nam.post(request, params.toString().toLatin1());
    //nam->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
}

bool Web::webUploadTodayLogAsMultiPart(QString todayLogPath)
{
    //QString wbPath = ui->lineEdit_wbPath->text();
    QHttpMultiPart * data = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"guid\""));
    //QUrlQuery params;
    //params.addQueryItem("guid", guid.toLatin1());
    QString body = QString("guid=") + guid + "\r\n\r\n";
    part.setBody( guid.toLatin1());
    //part.setBody( params.toString().toLatin1());
    //qDebug() << params.toString().toLatin1();
    //part.setRawHeader("guid", guid.toLatin1());
    //part.setRawHeader(body.toLatin1(), "");
    data->append(part);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("multipart/form-data; name=\"data\"; filename=\"log.zip\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-zip-compressed"));
    filePart.setRawHeader("Content-Transfer-Encoding","binary");

    QFile *file = new QFile(todayLogPath);
    if(file->open(QIODevice::ReadOnly) == false){
        emit msg("error today log file open: \"" + todayLogPath + "\"");
        return false;
    }
    //QByteArray ba = file->readAll();

    filePart.setBodyDevice(file);
    data->append(filePart);

//    QString wbPath = ui->lineEdit_wbPath->text();
    nam.post(QNetworkRequest(QUrl(wbPath+"/upload_logs.php")), data);

//    file->close();
    return true;
}

void Web::processUploadLogsReply(QNetworkReply* repl)
{
    if(repl->error() == QNetworkReply::NoError){
        QString ret = repl->readAll();
        if(ret.contains("ok")){
            emit msg("today log upload success");
        }
        else{
            emit msg("today log upload fail");
        }
        //qDebug() << "upload_logs finished success:" << repl->readAll();
    }
    else{
        emit msg("today log upload failed: \"" + repl->errorString() + ("\""));
        //qDebug() << "upload_logs finished " << repl->errorString();
    }
}

void Web::handlePostTasksTimeout()
{
    //QString wbPath = ui->lineEdit_wbPath->text();
    QUrlQuery params;
    params.addQueryItem("guid", guid.toLatin1());

    QNetworkRequest request;
    request.setUrl(QUrl(wbPath+"/tasks.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded"));

    nam.post(request, params.toString().toLatin1());
}

bool Web::isHttpRedirect(QNetworkReply *reply)
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return statusCode == 301 || statusCode == 302 || statusCode == 303
           || statusCode == 305 || statusCode == 307 || statusCode == 308;
}

QString Web::saveFileName(const QUrl &url)
{
    QString path = url.path();
    QString basename = QFileInfo(path).fileName();

    if (basename.isEmpty())
        basename = "download";

    if (QFile::exists(basename)) {
        // already exists, don't overwrite
        int i = 0;
        basename += '.';
        while (QFile::exists(basename + QString::number(i)))
            ++i;

        basename += QString::number(i);
    }

    //basename = "download\\" + basename;

    return basename;
}

bool Web::saveToDisk(const QString &filename, QIODevice *data)
{
    //QString rootPath = ui->lineEditRootPath->text();
    QDir().mkdir(dirStruct->downloadDir);
    QString filePath = dirStruct->downloadDir +"/"+ filename;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit msg(QString("Could not open %1 for writing: %2\n")
                .arg(filePath).arg(file.errorString()));
        return false;
    }

    file.write(data->readAll());
    file.close();

    return true;
}



void Web::processTasks(QString uq)
{
    //qDebug() << qPrintable(uq);
    QStringList strList = uq.split(";");

    foreach(QString part, strList) {
        //qDebug() << "task: " << part;
        processTask(part);
        //qDebug() << " ";
    }
}

void Web::processTask(QString task)
{
    QStringList taskParts = task.split("&");
    if(taskParts.count() != 3){
        QString errStr = "Err task: " + task;
        emit msg(errStr);
        return;
    }
    QString taskId = taskParts[0];
    QString taskType = taskParts[1];
    if(taskType == "install_program"){
        emit msg("install program task");
        //qDebug() << "install program task " << taskParts;
        QStringList pathParts = taskParts[2].split("!");
        //qDebug() << "install program task path " << pathParts;
        if(pathParts.count() != 2){
            QString errStr = "Err install task path: " + taskParts[2];
            emit msg(errStr);
            return;
        }
        dlProgTaskId = taskId;
        installProgram(pathParts[1]);
    }
    else if(taskType == "install_project"){
        emit msg("install project task");
        QStringList pathParts = taskParts[2].split("!");
        //qDebug() << "install program task path " << pathParts;
        if(pathParts.count() != 2){
            QString errStr = "Err install task path: " + taskParts[2];
            emit msg(errStr);
            return;
        }
        dlProjTaskId = taskId;
        installProgram(pathParts[1]);
    }
    else if(taskType == "uninstall_program"){
//        emit msg("uninstall program");
//        appendLogString("uninstall program task");
//        qDebug() << "uninstall program task";
//        uninstallProgram("");
    }
    else if(taskType == "uninstall_project"){
//        emit msg("uninstall project");
//        appendLogString("uninstall project task");
//        qDebug() << "uninstall project task";
    }
    else if(taskType == "upload"){
//        emit msg("upload");
//        uploadPath();
//        appendLogString("upload task");
//        qDebug() << "upload task";
    }
    else if(taskType == "delete"){
//        emit msg("delete");
//        appendLogString("delete task");
//        qDebug() << "delete task";
    }
    else if(taskType == "restart"){
        emit msg("restart task");
        restart(taskId);
    }
}

bool Web::processDownloaded(QNetworkReply* repl)
{
    //qDebug() << "unknown req: " << qPrintable(readed);
//            QString filename = saveFileName(url);
//            if (saveToDisk(filename, reply)) {
//                printf("Download of %s succeeded (saved to %s)\n",
//                       url.toEncoded().constData(), qPrintable(filename));
//            }

    QString reqUrl = repl->url().toString();

    QString dirSuffix="";
    if(reqUrl.contains("/programs/")==true){
        dirSuffix = "programs";
    }
    else if(reqUrl.contains("/projects/")==true){
        dirSuffix = "projects";
    }
    QUrl url = repl->url();
    bool ret = false;
    if(repl->error()){
//                qDebug("Download of %s failed: %s\n",
//                        url.toEncoded().constData(),
//                        qPrintable(repl->errorString()));
        emit msg(QString("Download of %1 failed: %2")
                 .arg(url.toEncoded().constData())
                 .arg(qPrintable(repl->errorString())));
        ret = false;
    }
    else{
        if (isHttpRedirect(repl)){
            emit msg("Request was redirected");
        }
        else{
            QString filename = saveFileName(url);
            QString filenameInDownloadDir = dirSuffix +"_"+filename;
            if (saveToDisk(filenameInDownloadDir, repl)) {
                emit msg(QString("Download of %1 succeeded (saved to %2)")
                         .arg(url.toString()).arg(filenameInDownloadDir));

                //QString rootPath = ui->lineEditRootPath->text();
                QString filePath = dirStruct->downloadDir +"/"+ filenameInDownloadDir;

                QFileInfo  fi(filename);
                QString fileOutPath = dirStruct->rootDir + "/" + dirSuffix + "/" + fi.baseName();
                QDir().mkpath(fileOutPath);

                unZip(filePath, fileOutPath);
            }
        }
        ret = true;
    }
    return ret;
}

void Web::installProgram(QString path)
{
    //qDebug(qPrintable(QString("install task: ") + path));
    //QString wbPath = ui->lineEdit_wbPath->text();
    QUrl fileUrl(wbPath+path);
    emit msg("download path: \"" + fileUrl.toString() + "\"");
    //qDebug() << fileUrl;
    QNetworkRequest request(fileUrl);

    QNetworkReply *repl =  nam.get(request);
    connect(repl, &QNetworkReply::downloadProgress,
            [=](qint64 bytesReceived, qint64 bytesTotal){
        //qDebug("download process %d %d", bytesReceived, bytesTotal);
        emit msg(QString("download process - recvd:%1, total:%2").arg(bytesReceived).arg(bytesTotal));
    });


}

void Web::uninstallProgram(QString path)
{

}

void Web::restart(QString taskId)
{
    postProgress(taskId);
    emit msg(QString("restarting"));
    QProcess proc;
    proc.startDetached("shutdown /r");
}

void Web::uploadPath()
{

}

