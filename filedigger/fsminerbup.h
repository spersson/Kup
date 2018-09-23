#ifndef FSMINERBUP_H
#define FSMINERBUP_H

#include "fsminer.h"

class FsMinerBup : public FsMiner
{
    Q_OBJECT
public:
    explicit FsMinerBup(const QString &mountPath, QObject *parent = nullptr);
    virtual QString pathToLatest() override;

protected:
    virtual void doProcess() override;
};

#endif // FSMINERBUP_H
