#ifndef USERATTRIBUTESMANAGER_H
#define USERATTRIBUTESMANAGER_H

#include <QTMegaListener.h>

#include <QObject>
#include <QMultiMap>
#include <QSharedPointer>

#include <memory>

namespace UserAttributes
{
class AttributeRequest : public QObject
{
    Q_OBJECT

public:
    struct RequestInfo
    {
        struct ParamInfo{
            QList<int> mNoRetryErrCodes; //if received err code is not in the list failed will be changed to true
            bool mNeedsRetry = true;
            bool mIsPending = false;
            ParamInfo(const std::function<void()>& func, QList<int> errCodes)
                : mNoRetryErrCodes(errCodes)
                , requestFunc(func)
            {
                Q_ASSERT(!errCodes.isEmpty());
            }
            ParamInfo(const std::function<void()>& func)
                : requestFunc(func)
            {
            }

            void setNeedsRetry(int errCode);
            void setPending(bool isPending);
            std::function<void()> requestFunc;
        };

        QMap<int, QSharedPointer<ParamInfo>> mParamInfo; //key: params available for this request
        QMap<int64_t, int> mChangedTypes;

        RequestInfo(QMap<int, QSharedPointer<ParamInfo>> pInfo, QMap<int64_t, int> cTypes)
            : mParamInfo(pInfo)
            , mChangedTypes(cTypes)
        {
        }
    };
    typedef AttributeRequest::RequestInfo::ParamInfo ParamInfo;
    typedef QMap<int, QSharedPointer<ParamInfo>> ParamInfoMap;

    AttributeRequest(const QString& userEmail) : mUserEmail(userEmail), mRequestInfo(QMap<int, QSharedPointer<ParamInfo>>(), QMap<int64_t, int>()){}

    virtual void onRequestFinish(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError *e) = 0;
    virtual void requestAttribute()= 0;
    void forceRequestAttribute() const;
    virtual bool isAttributeReady() const = 0;
    virtual RequestInfo fillRequestInfo() = 0;
    bool attributeRequestNeedsRetry(int attribute) const;
    bool isAttributeRequestPending(int attribute) const;
    bool isRequestPending() const;
    const QString& getEmail() const {return mUserEmail;}
    void requestUserAttribute(int attribute);
    const RequestInfo& getRequestInfo() const {return mRequestInfo;}
    void initRequestInfo(){mRequestInfo = fillRequestInfo();}
protected:
    QString mUserEmail;
    RequestInfo mRequestInfo;
};

class UserAttributesManager : public mega::MegaListener
{
public:
    static UserAttributesManager& instance()
    {
        static UserAttributesManager    instance;
        return instance;
    }

    void reset();

    template <typename AttributeClass>
    std::shared_ptr<AttributeClass> requestAttribute(const char* user_email = nullptr)
    {
        QString userEmail = QString::fromUtf8(user_email);
        QString mapKey = getKey(userEmail);

        auto classType = QString::fromUtf8(AttributeClass::staticMetaObject.className());

        auto userRequests = mRequests.values(mapKey);
        foreach(auto& request, userRequests)
        {
            auto requestType = QString::fromUtf8(request->metaObject()->className());
            if(requestType == classType)
            {
                foreach(auto paramType, request->getRequestInfo().mParamInfo.keys())
                {
                    request->requestUserAttribute(paramType);
                }
                return std::dynamic_pointer_cast<AttributeClass>(request);
            }
        }

        auto request = std::make_shared<AttributeClass>(userEmail);
        request->initRequestInfo();
        mRequests.insert(mapKey, std::static_pointer_cast<AttributeRequest>(request));
        request->requestAttribute();

        return request;
    }

    void updateEmptyAttributesByUser(const char* user_email);

private:
    friend class AttributeRequest;

    void onRequestFinish(mega::MegaApi *api, mega::MegaRequest *incoming_request, mega::MegaError *e) override;
    void onUsersUpdate(mega::MegaApi *, mega::MegaUserList *users) override;

    void forceRequestAttribute(const AttributeRequest*) const;

    explicit UserAttributesManager();
    QString getKey(const QString& userEmail) const;

    std::unique_ptr<mega::QTMegaListener> mDelegateListener;
    QMultiMap<QString, std::shared_ptr<AttributeRequest>> mRequests;
};
}

#endif // USERATTRIBUTESMANAGER_H
