#ifndef SEAFILE_CLIETN_FILEBROWSER_TAKS_H
#define SEAFILE_CLIETN_FILEBROWSER_TAKS_H

#include <QObject>
#include <QUrl>

#include "account.h"

class QTemporaryFile;
class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QSslError;

class FileServerTask;
class SeafileApiRequest;

template<typename T> class QList;

/**
 * Handles file upload/download using seafile web api.
 * The task contains two phases:
 *
 * First, we need to get the upload/download link for seahub.
 * Second, we upload/download file to seafile fileserver with that link using
 * a `FileServerTask`.
 *
 *
 * In second phase, the FileServerTask is moved to a worker thread to execute,
 * since we will do blocking file IO in that task.
 *
 */
class FileNetworkTask : public QObject {
    Q_OBJECT
public:
    enum TaskType {
        Upload,
        Download,
    };

    FileNetworkTask(const Account& account,
                    const QString& repo_id,
                    const QString& path,
                    const QString& local_path);

    virtual ~FileNetworkTask();

    // accessors
    virtual TaskType type() const = 0;
    QString repoId() const { return repo_id_; };
    QString path() const { return path_; };
    QString localFilePath() const { return local_path_; }
    QString fileName() const;

public slots:
    virtual void start();
    virtual void cancel();

signals:
    void progressUpdate(qint64 transferred, qint64 total);
    void finished(bool success);

protected slots:
    virtual void onLinkGet(const QString& link);
    virtual void onGetLinkFailed();
    virtual void startFileServerTask(const QString& link);
    virtual void onFinished(bool success);

protected:
    virtual void createGetLinkRequest() = 0;
    virtual void createFileServerTask(const QString& link) = 0;

    FileServerTask *fileserver_task_;
    SeafileApiRequest *get_link_req_;

    Account account_;
    QString repo_id_;
    QString path_;
    QString local_path_;

    static QThread *worker_thread_;
};


/**
 * The downloaded file is first written to a tmp location, then moved
 * to its final location.
 */
class FileDownloadTask : public FileNetworkTask {
    Q_OBJECT
public:
    FileDownloadTask(const Account& account,
                     const QString& repo_id,
                     const QString& path,
                     const QString& local_path);

    TaskType type() const { return Download; }
    QString fileId() const { return file_id_; }

protected:
    void createFileServerTask(const QString& link);
    void createGetLinkRequest();

protected slots:
    void onLinkGet(const QString& link);

private:
    QString file_id_;
};

class FileUploadTask : public FileNetworkTask {
    Q_OBJECT
public:
    FileUploadTask(const Account& account,
                   const QString& repo_id,
                   const QString& path,
                   const QString& local_path);

    TaskType type() const { return Upload; }

protected:
    void createFileServerTask(const QString& link);
    void createGetLinkRequest();
};

/**
 * Handles raw file download/upload with seafile file server.
 *
 * This is the base class for file upload/download task. The task runs in a
 * seperate thread, which means we can not invoke non-const methods of it from
 * the main thread. To start the task: use QMetaObject::invokeMethod to call
 * the start() member function. task. The same for canceling the task.
 */
class FileServerTask : public QObject {
    Q_OBJECT
public:
    FileServerTask(const QUrl& url,
                 const QString& local_path);
    virtual ~FileServerTask();

signals:
    void progressUpdate(qint64 transferred, qint64 total);
    void finished(bool success);

public slots:
    virtual void start() = 0;
    void cancel();

protected slots:
    void onSslErrors(const QList<QSslError>& errors);

protected:
    static QNetworkAccessManager *network_mgr_;
    QUrl url_;
    QString local_path_;

    QNetworkReply *reply_;
    bool canceled_;
};

class GetFileTask : public FileServerTask {
    Q_OBJECT
public:
    GetFileTask(const QUrl& url, const QString& local_path);
    ~GetFileTask();

public slots:
    void start();

private slots:
    void httpReadyRead();
    void httpRequestFinished();

private:
    QTemporaryFile *tmp_file_;
};

class PostFileTask : public FileServerTask {
    Q_OBJECT
public:
    PostFileTask(const QUrl& url,
                 const QString& parent_dir,
                 const QString& local_path);
    ~PostFileTask();

public slots:
    void start();

protected slots:
    void httpRequestFinished();

private:
    QString parent_dir_;
    QFile *file_;
};

#endif // SEAFILE_CLIETN_FILEBROWSER_TAKS_H
