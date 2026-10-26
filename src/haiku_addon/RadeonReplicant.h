#ifndef RADEON_REPLICANT_H
#define RADEON_REPLICANT_H

#include <View.h>
#include <Archivable.h>

class RadeonReplicant : public BView, public BArchivable {
public:
    RadeonReplicant(BRect frame);
    RadeonReplicant(BMessage* archive);
    
    static RadeonReplicant* Instantiate(BMessage* archive);
    virtual status_t Archive(BMessage* archive, bool deep = true) const;
    
    virtual void Draw(BRect updateRect);
    virtual void MouseDown(BPoint point);
};

#endif
