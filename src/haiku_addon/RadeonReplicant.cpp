#include "RadeonReplicant.h"
#include <Message.h>
#include <String.h>
#include <stdio.h>

RadeonReplicant::RadeonReplicant(BRect frame)
    : BView(frame, "RadeonReplicant", B_FOLLOW_NONE, B_WILL_DRAW)
{
    SetViewColor(B_TRANSPARENT_COLOR);
}

RadeonReplicant::RadeonReplicant(BMessage* archive)
    : BView(archive)
{
}

RadeonReplicant* RadeonReplicant::Instantiate(BMessage* archive)
{
    if (!validate_instantiation(archive, "RadeonReplicant"))
        return NULL;
    return new RadeonReplicant(archive);
}

status_t RadeonReplicant::Archive(BMessage* archive, bool deep) const
{
    status_t status = BView::Archive(archive, deep);
    if (status == B_OK)
        status = archive->AddString("add_on", "radeon_legacy_driver");
    return status;
}

void RadeonReplicant::Draw(BRect updateRect)
{
    SetHighColor(0, 255, 0); // Green = active
    FillRect(Bounds());
    SetHighColor(0, 0, 0);
    DrawString("Radeon", BPoint(2, Bounds().Height() - 2));
}

void RadeonReplicant::MouseDown(BPoint point)
{
    printf("RadeonReplicant clicked!\n");
}
