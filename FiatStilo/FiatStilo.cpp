#include "FiatStilo.hpp"

#define DEBUG false


FiatStilo::~FiatStilo()
{
    if (this->vehicle)
        delete this->vehicle;
}

bool FiatStilo::init(ICANBus* canbus){
    if (this->arbiter) {
        this->aa_handler = this->arbiter->android_auto().handler;
        this->vehicle = new Vehicle(*this->arbiter);
        this->vehicle->setObjectName("Fiat Stilo");
        this->vehicle->pressure_init("psi", 30);
        this->vehicle->disable_sensors();
        this->vehicle->rotate(270);

        if(DEBUG)
            this->debug = new DebugWindow(*this->arbiter);

        canbus->registerFrameHandler(0x60D, [this](QByteArray payload){this->monitorHeadlightStatus(payload);});
        canbus->registerFrameHandler(0x551, [this](QByteArray payload){this->engineUpdate(payload);});
        canbus->registerFrameHandler(0x385, [this](QByteArray payload){this->tpmsUpdate(payload);});
        canbus->registerFrameHandler(0x354, [this](QByteArray payload){this->brakePedalUpdate(payload);});
        canbus->registerFrameHandler(0x002, [this](QByteArray payload){this->steeringWheelUpdate(payload);});
        return true;
    }
    else{
        return false;
    }
    

}

QList<QWidget *> FiatStilo::widgets()
{
    QList<QWidget *> tabs;
    tabs.append(this->vehicle);
    if(DEBUG)
        tabs.append(this->debug);
    return tabs;
}




void FiatStilo::engineUpdate(QByteArray payload){
    if((payload.at(3) == 0x80)) engineRunning = true;
    else
    {
        if(engineRunning)
            this->aa_handler->injectButtonPress(aasdk::proto::enums::ButtonCode::PAUSE);
        engineRunning = false;
    }
}



void FiatStilo::steeringWheelUpdate(QByteArray payload){
    uint16_t rawAngle = payload.at(1);
    rawAngle = rawAngle<<8;
    rawAngle |= payload.at(0);
    int degAngle = 0;
    if(rawAngle>32767) degAngle = -((65535-rawAngle)/10);
    else degAngle = rawAngle/10;
    degAngle = degAngle/16.4;
    this->vehicle->wheel_steer(degAngle);
}

//354
//(A, B, C, D, E, F, G, H)
// G - brake pedal
//     4 - off
//     20 - pressed a bit

void FiatStilo::brakePedalUpdate(QByteArray payload){
    bool brakePedalUpdate = false;
    if((payload.at(6) == 20)) brakePedalUpdate = true;
    this->vehicle->taillights(brakePedalUpdate);   
}

// HEADLIGHTS AND DOORS
// 60D
// FIRST BYTE:
// |unknown|RR_DOOR|RL_DOOR|FR_DOOR|FL_DOOR|SIDE_LIGHTS|HEADLIGHTS|unknown|
// SECOND BYTE:
// |unknown|unknown|unknown|unknown|unknown|left turn signal light|right turn signal light|FOGLIGHTS|
// OTHERS UNKNOWN

void FiatStilo::monitorHeadlightStatus(QByteArray payload){
    if((payload.at(0)>>1) & 1){
        //headlights are ON - turn to dark mode
        if(this->arbiter->theme().mode == Session::Theme::Light){
            this->arbiter->set_mode(Session::Theme::Dark);
        this->vehicle->headlights(true);
        }
    }
    else{
        //headlights are off or not fully on (i.e. sidelights only) - make sure is light mode
        if(this->arbiter->theme().mode == Session::Theme::Dark){
            this->arbiter->set_mode(Session::Theme::Light);
        this->vehicle->headlights(false);
        }
    }
    bool rrDoorUpdate = (payload.at(0) >> 6) & 1;
    bool rlDoorUpdate = (payload.at(0) >> 5) & 1;
    bool frDoorUpdate = (payload.at(0) >> 4) & 1;
    bool flDoorUpdate = (payload.at(0) >> 3) & 1;
    this->vehicle->door(Position::BACK_RIGHT, rrDoorUpdate);
    this->vehicle->door(Position::BACK_LEFT, rlDoorUpdate);
    this->vehicle->door(Position::FRONT_RIGHT, frDoorUpdate);
    this->vehicle->door(Position::FRONT_LEFT, flDoorUpdate);

    bool rTurnUpdate = (payload.at(1)>>6) & 1;
    bool lTurnUpdate = (payload.at(1)>>5) & 1;
    this->vehicle->indicators(Position::LEFT, lTurnUpdate);
    this->vehicle->indicators(Position::RIGHT, rTurnUpdate);
}


bool oldStatus = true;


DebugWindow::DebugWindow(Arbiter &arbiter, QWidget *parent) : QWidget(parent)
{
    this->setObjectName("Debug");
}
