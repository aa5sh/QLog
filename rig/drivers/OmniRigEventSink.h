#ifndef OMNIRIGEVENTSINK_H
#define OMNIRIGEVENTSINK_H

#include <windows.h>
#include <ocidl.h>

// Generic IDispatch sink for OmniRig event interfaces.
// Owner class must implement rigTypeChange(int), rigStatusChange(int) and rigParamsChange(int, int).
template <typename Owner, typename EventInterface>
class OmniRigEventSink : public IDispatch
{
public:
    explicit OmniRigEventSink(Owner *owner) :
        refCount(1), owner(owner) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        if ( !ppvObject ) return E_POINTER;

        if ( riid == IID_IUnknown
            || riid == IID_IDispatch
            || riid == __uuidof(EventInterface) )
        {
            *ppvObject = static_cast<IDispatch *>(this);
            AddRef();
            return S_OK;
        }

        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG r = InterlockedDecrement(&refCount);
        if ( r == 0 ) delete this;
        return r;
    }

    // IDispatch
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo) override
    {
        if ( pctinfo ) *pctinfo = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo **) override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR *, UINT, LCID, DISPID *) override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember,
                                     REFIID,
                                     LCID,
                                     WORD,
                                     DISPPARAMS *pDispParams,
                                     VARIANT *,
                                     EXCEPINFO *,
                                     UINT *) override
    {
        qCDebug(runtime) << "OmniRig event: dispId =" << dispIdMember
                         << "argc =" << (pDispParams ? (int)pDispParams->cArgs : -1);

        if ( !owner ) return S_OK;

        // DispIDs from IDL:
        // 1 VisibleChange()
        // 2 RigTypeChange(long RigNumber)
        // 3 StatusChange(long RigNumber)
        // 4 ParamsChange(long RigNumber, long Params)
        // 5 CustomReply(long RigNumber, VARIANT Command, VARIANT Reply)
        switch (dispIdMember)
        {
        case 1: // VisibleChange
            // not-used
            break;

        case 2: // RigTypeChange
            if ( pDispParams && pDispParams->cArgs == 1 )
                owner->rigTypeChange((int)pDispParams->rgvarg[0].lVal);
            break;

        case 3: // StatusChange
            if ( pDispParams && pDispParams->cArgs == 1 )
                owner->rigStatusChange((int)pDispParams->rgvarg[0].lVal);
            break;

        case 4: // ParamsChange(RigNumber, Params)
            if ( pDispParams && pDispParams->cArgs == 2 )
            {
                long params = pDispParams->rgvarg[0].lVal;
                long rig    = pDispParams->rgvarg[1].lVal;
                owner->rigParamsChange((int)rig, (int)params);
            }
            break;

        case 5: // CustomReply
            // not-used
            break;

        default:
            break;
        }

        return S_OK;
    }

private:
    LONG   refCount;
    Owner *owner;
};

#endif // OMNIRIGEVENTSINK_H
