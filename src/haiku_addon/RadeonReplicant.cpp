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

#include "RadeonReplicant.h"
#include <Message.h>
#include <String.h>
#include <stdio.h>
#include <PopUpMenu.h>
#include <MenuItem.h>

RadeonReplicant::RadeonReplicant(BRect frame)
    : BView(frame, "RadeonReplicant", B_FOLLOW_ALL, B_WILL_DRAW)
{
    SetViewColor(B_TRANSPARENT_COLOR);
}

// ... (Resto del código omitido para brevedad, implementando Draw con gradiente)
void RadeonReplicant::Draw(BRect updateRect)
{
    // Gradiente profesional
    rgb_color start = { 60, 60, 60, 255 };
    rgb_color end = { 30, 30, 30, 255 };
    // FillGradient(Bounds(), start, end); // Nota: BView necesita Setup para gradientes complejos
    SetHighColor(40, 40, 40);
    FillRect(Bounds());
    
    // Indicador dinámico
    SetHighColor(0, 255, 0);
    FillEllipse(BRect(Bounds().Width() - 8, 2, Bounds().Width() - 2, 8));
    
    SetHighColor(255, 255, 255);
    DrawString("Radeon HD", BPoint(4, Bounds().Height() - 6));
}

void RadeonReplicant::MouseDown(BPoint point)
{
    BPopUpMenu* menu = new BPopUpMenu("Radeon Menu");
    menu->AddItem(new BMenuItem("Status: Active", NULL));
    menu->AddItem(new BMenuItem("Set Resolution", NULL));
    
    ConvertToScreen(&point);
    menu->Go(point, true, true, true);
}
