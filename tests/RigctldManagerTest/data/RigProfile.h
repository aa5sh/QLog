#ifndef TEST_DATA_RIGPROFILE_H
#define TEST_DATA_RIGPROFILE_H

#include <QString>

// Minimal RigProfile stub for testing RigctldManager

class RigProfile
{
public:
    enum rigPortType
    {
        SERIAL_ATTACHED,
        NETWORK_ATTACHED,
        SPECIAL_OMNIRIG_ATTACHED
    };

    RigProfile() {
        model = 1; netport = 0; baudrate = 0;
        databits = 0; stopbits = 0.0;
        shareRigctld = false; rigctldPort = 4532;
        civAddr = -1;
    }

    QString profileName;
    qint32 model;
    QString portPath;
    QString hostname;
    quint16 netport;
    quint32 baudrate;
    quint8 databits;
    float stopbits;
    QString flowcontrol;
    QString parity;
    QString pttType;
    QString pttPortPath;
    QString dtr;
    QString rts;
    qint16 civAddr;
    bool shareRigctld;
    quint16 rigctldPort;
    QString rigctldPath;
    QString rigctldArgs;

    rigPortType getPortType() const
    {
        if (!hostname.isEmpty() && portPath.isEmpty())
            return NETWORK_ATTACHED;
        return SERIAL_ATTACHED;
    }
};

#endif // TEST_DATA_RIGPROFILE_H
