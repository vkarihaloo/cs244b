/*
 *   FILE: toplevel.c
 * AUTHOR: Wei Shi (weishi@stanford.edu)
 *   DATE: March 31 23:59:59 PST 2013
 *  DESCR:
 */

/* #define DEBUG */

#include "main.h"
#include <string>
#include "mazewar.h"

static bool		updateView;	/* true if update needed */
MazewarInstance::Ptr M;

/* Use this socket address to send packets to the multi-cast group. */
static Sockaddr         groupAddr;
#define MAX_OTHER_RATS  (MAX_RATS - 1)

static uint32_t seqNum = 0;
static uint8_t seqMis = 0;

static enum Phase phase = Join;

int main(int argc, char *argv[])
{
    Loc x(1);
    Loc y(5);
    Direction dir(0);
    char *ratName;

    signal(SIGHUP, quit);
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    getName("Welcome to CS244B MazeWar!\n\nYour Name", &ratName);
    if(strlen(ratName) < MAX_RAT_NAME) {
        ratName[strlen(ratName) - 1] = 0;
    } else {
        ratName[MAX_RAT_NAME - 1] = 0;
    }

    M = MazewarInstance::mazewarInstanceNew(string(ratName));
    MazewarInstance *a = M.ptr();
    strncpy(M->myName_, ratName, NAMESIZE);
    free(ratName);

    MazeInit(argc, argv);

    NewPosition(M);

    play();

    return 0;
}


/* ----------------------------------------------------------------------- */

void
play(void)
{
    MWEvent		event;
    MW244BPacket	incoming;

    event.eventDetail = &incoming;

    struct timeval startup;
    gettimeofday(&startup, NULL);
    while (TRUE) {
        NextEvent(&event, M->theSocket());
        /* Only listen for heartbeat during Phase.Join */
        if(phase == Join) {
            if(event.eventType == EVENT_NETWORK) {
                processPacket(&event);
            }
            checkJoinComplete(startup);
            continue;
        }
        /* Init after Join completes */
        if(phase == Init) {
            cout << "init" << endl;
            M->myRatIdIs(generateId());
            phase = Play;
        }
        /* Process all inputs during Phase.Play*/
        if (!M->peeking())
            switch(event.eventType) {
            case EVENT_A:
                aboutFace();
                break;

            case EVENT_S:
                leftTurn();
                break;

            case EVENT_D:
                forward();
                break;

            case EVENT_F:
                rightTurn();
                break;

            case EVENT_BAR:
                backward();
                break;

            case EVENT_LEFT_D:
                peekLeft();
                break;

            case EVENT_MIDDLE_D:
                shoot();
                break;

            case EVENT_RIGHT_D:
                peekRight();
                break;

            case EVENT_NETWORK:
                processPacket(&event);
                break;

            case EVENT_INT:
                quit(0);
                break;

            }
        else
            switch (event.eventType) {
            case EVENT_RIGHT_U:
            case EVENT_LEFT_U:
                peekStop();
                break;

            case EVENT_NETWORK:
                processPacket(&event);
                break;
            }

        ratStates();		/* clean house */

        manageMissiles();

        DoViewUpdate();

        /* Any info to send over network? */
        sendHeartbeat();

    }
}

/* ----------------------------------------------------------------------- */

static	Direction	_aboutFace[NDIRECTION] = {SOUTH, NORTH, WEST, EAST};
static	Direction	_leftTurn[NDIRECTION] =	{WEST, EAST, NORTH, SOUTH};
static	Direction	_rightTurn[NDIRECTION] = {EAST, WEST, SOUTH, NORTH};

void
aboutFace(void)
{
    M->dirIs(_aboutFace[MY_DIR]);
    updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void
leftTurn(void)
{
    M->dirIs(_leftTurn[MY_DIR]);
    updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void
rightTurn(void)
{
    M->dirIs(_rightTurn[MY_DIR]);
    updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

/* remember ... "North" is to the right ... positive X motion */

void
forward(void)
{
    register int	tx = MY_X_LOC;
    register int	ty = MY_Y_LOC;

    switch(MY_DIR) {
    case NORTH:
        if (!M->maze_[tx + 1][ty])	tx++;
        break;
    case SOUTH:
        if (!M->maze_[tx - 1][ty])	tx--;
        break;
    case EAST:
        if (!M->maze_[tx][ty + 1])	ty++;
        break;
    case WEST:
        if (!M->maze_[tx][ty - 1])	ty--;
        break;
    default:
        MWError("bad direction in Forward");
    }
    if(isConflictPosition(Loc(tx), Loc(ty))) {
        printf("forward aborted\n");
        return;
    }
    if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
        M->xlocIs(Loc(tx));
        M->ylocIs(Loc(ty));
        updateView = TRUE;
    }
}

/* ----------------------------------------------------------------------- */

void backward()
{
    register int	tx = MY_X_LOC;
    register int	ty = MY_Y_LOC;

    switch(MY_DIR) {
    case NORTH:
        if (!M->maze_[tx - 1][ty])	tx--;
        break;
    case SOUTH:
        if (!M->maze_[tx + 1][ty])	tx++;
        break;
    case EAST:
        if (!M->maze_[tx][ty - 1])	ty--;
        break;
    case WEST:
        if (!M->maze_[tx][ty + 1])	ty++;
        break;
    default:
        MWError("bad direction in Backward");
    }
    if(isConflictPosition(Loc(tx), Loc(ty))) {
        printf("backward aborted\n");
        return;
    }
    if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
        M->xlocIs(Loc(tx));
        M->ylocIs(Loc(ty));
        updateView = TRUE;
    }
}

/* ----------------------------------------------------------------------- */

void peekLeft()
{
    M->xPeekIs(MY_X_LOC);
    M->yPeekIs(MY_Y_LOC);
    M->dirPeekIs(MY_DIR);

    switch(MY_DIR) {
    case NORTH:
        if (!M->maze_[MY_X_LOC + 1][MY_Y_LOC]) {
            M->xPeekIs(MY_X_LOC + 1);
            M->dirPeekIs(WEST);
        }
        break;

    case SOUTH:
        if (!M->maze_[MY_X_LOC - 1][MY_Y_LOC]) {
            M->xPeekIs(MY_X_LOC - 1);
            M->dirPeekIs(EAST);
        }
        break;

    case EAST:
        if (!M->maze_[MY_X_LOC][MY_Y_LOC + 1]) {
            M->yPeekIs(MY_Y_LOC + 1);
            M->dirPeekIs(NORTH);
        }
        break;

    case WEST:
        if (!M->maze_[MY_X_LOC][MY_Y_LOC - 1]) {
            M->yPeekIs(MY_Y_LOC - 1);
            M->dirPeekIs(SOUTH);
        }
        break;

    default:
        MWError("bad direction in PeekLeft");
    }

    /* if any change, display the new view without moving! */

    if ((M->xPeek() != MY_X_LOC) || (M->yPeek() != MY_Y_LOC)) {
        M->peekingIs(TRUE);
        updateView = TRUE;
    }
}

/* ----------------------------------------------------------------------- */

void peekRight()
{
    M->xPeekIs(MY_X_LOC);
    M->yPeekIs(MY_Y_LOC);
    M->dirPeekIs(MY_DIR);

    switch(MY_DIR) {
    case NORTH:
        if (!M->maze_[MY_X_LOC + 1][MY_Y_LOC]) {
            M->xPeekIs(MY_X_LOC + 1);
            M->dirPeekIs(EAST);
        }
        break;

    case SOUTH:
        if (!M->maze_[MY_X_LOC - 1][MY_Y_LOC]) {
            M->xPeekIs(MY_X_LOC - 1);
            M->dirPeekIs(WEST);
        }
        break;

    case EAST:
        if (!M->maze_[MY_X_LOC][MY_Y_LOC + 1]) {
            M->yPeekIs(MY_Y_LOC + 1);
            M->dirPeekIs(SOUTH);
        }
        break;

    case WEST:
        if (!M->maze_[MY_X_LOC][MY_Y_LOC - 1]) {
            M->yPeekIs(MY_Y_LOC - 1);
            M->dirPeekIs(NORTH);
        }
        break;

    default:
        MWError("bad direction in PeekRight");
    }

    /* if any change, display the new view without moving! */

    if ((M->xPeek() != MY_X_LOC) || (M->yPeek() != MY_Y_LOC)) {
        M->peekingIs(TRUE);
        updateView = TRUE;
    }
}

/* ----------------------------------------------------------------------- */

void peekStop()
{
    M->peekingIs(FALSE);
    updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void shoot()
{
    timeval cur;
    if(M->hasMissile()) {
        return;
    }
    M->scoreIs( M->score().value() - 1 );
    UpdateScoreCard(MY_RAT_INDEX);

    gettimeofday(&cur, NULL);
    M->hasMissileIs(true);
    M->xMissileIs(MY_X_LOC);
    M->yMissileIs(MY_Y_LOC);
    M->dirMissileIs(MY_DIR);
    M->updateMissileIs(cur);


}

/* ----------------------------------------------------------------------- */

/*
 * Exit from game, clean up window
 */

void quit(int sig)
{

    StopWindow();
    exit(0);
}


/* ----------------------------------------------------------------------- */

void NewPosition(MazewarInstance::Ptr m)
{
    Loc newX(0);
    Loc newY(0);
    Direction dir(0); /* start on occupied square */

    while (M->maze_[newX.value()][newY.value()]) {
        /* MAZE[XY]MAX is a power of 2 */
        newX = Loc(random() & (MAZEXMAX - 1));
        newY = Loc(random() & (MAZEYMAX - 1));

        /* In real game, also check that square is
           unoccupied by another rat */
    }

    /* prevent a blank wall at first glimpse */

    if (!m->maze_[(newX.value()) + 1][(newY.value())]) dir = Direction(NORTH);
    if (!m->maze_[(newX.value()) - 1][(newY.value())]) dir = Direction(SOUTH);
    if (!m->maze_[(newX.value())][(newY.value()) + 1]) dir = Direction(EAST);
    if (!m->maze_[(newX.value())][(newY.value()) - 1]) dir = Direction(WEST);

    m->xlocIs(newX);
    m->ylocIs(newY);
    m->dirIs(dir);
}

/* ----------------------------------------------------------------------- */

void MWError(char *s)

{
    StopWindow();
    fprintf(stderr, "CS244BMazeWar: %s\n", s);
    perror("CS244BMazeWar");
    exit(-1);
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
Score GetRatScore(RatIndexType ratId)
{
    if (ratId.value() == 	MY_RAT_INDEX) {
        return(M->score());
    } else {
        Rat r = M->rat(ratId);
        return r.score;
    }
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
const char *GetRatName(RatIndexType ratId)
{
    if (ratId.value() ==	MY_RAT_INDEX) {
        return(M->myName_);
    } else {
        Rat r = M->rat(ratId);
        return r.name.c_str();
    }
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
void ratStates()
{
    /* In our sample version, we don't know about the state of any rats over
       the net, so this is a no-op */
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
void manageMissiles()
{
    register int	oldtx = MY_X_MIS;
    register int	oldty = MY_Y_MIS;
    register int	tx = oldtx;
    register int	ty = oldty;
    timeval cur;
    if(!M->hasMissile()) {
        return;
    }
    gettimeofday(&cur, NULL);
    if(timediff(cur, M->updateMissile()) < MISSILE_SPEED) {
        return;
    } else {
        M->updateMissileIs(cur);
    }
    /* TODO: check for hits */
    switch(MY_DIR_MIS) {
    case NORTH:
        if (!M->maze_[tx + 1][ty]) {
            tx++;
        } else {
            M->hasMissileIs(false);
        }
        break;
    case SOUTH:
        if (!M->maze_[tx - 1][ty]) {
            tx--;
        } else {
            M->hasMissileIs(false);
        }
        break;
    case EAST:
        if (!M->maze_[tx][ty + 1]) {
            ty++;
        } else {
            M->hasMissileIs(false);
        }
        break;
    case WEST:
        if (!M->maze_[tx][ty - 1])	{
            ty--;
        } else {
            M->hasMissileIs(false);
        }
        break;
    default:
        MWError("bad direction in Managing missile");
    }
    if (M->hasMissile()) {
        M->xMissileIs(Loc(tx));
        M->yMissileIs(Loc(ty));
        showMissile(MY_X_MIS, MY_Y_MIS, MY_DIR_MIS, Loc(oldtx), Loc(oldty), true);
    } else {
        clearSquare(Loc(oldtx), Loc(oldty));
    }
    updateView = TRUE;

}

/* ----------------------------------------------------------------------- */

void DoViewUpdate()
{
    if (updateView) {	/* paint the screen */
        ShowPosition(MY_X_LOC, MY_Y_LOC, MY_DIR);
        //ShowAllPositions();
        if (M->peeking())
            ShowView(M->xPeek(), M->yPeek(), M->dirPeek());
        else
            ShowView(MY_X_LOC, MY_Y_LOC, MY_DIR);
        updateView = FALSE;
    }
}

/* ----------------------------------------------------------------------- */
/*
 * boardcast packet to UDP group
 */

void sendPacket(mazePacket *pack)
{
    uint8_t buf[64];
    memset(buf, 0, sizeof(64));
    pack->serialize(buf, sizeof(buf));
    //for(int j = 0; j < pack->size(); j++) {
    //    printf("%02X", buf[j]);
    //}
    if (sendto((int)M->theSocket(), &buf, pack->size(), 0,
               (sockaddr *) &groupAddr, sizeof(Sockaddr)) < 0) {
        MWError("Sample error");
    }
}

void sendHeartbeat()
{
    heartbeat *hb = (heartbeat *)packetFactory::createPacket(TYPE_HEARTBEAT);
    hb->id = M->myRatId().value();
    hb->seqNum = seqNum++;
    hb->xLoc = MY_X_LOC;
    hb->yLoc = MY_Y_LOC;
    hb->dir = MY_DIR;
    hb->score = M->score().value();
    hb->seqMis = seqMis;
    if(M->hasMissile()) {
        hb->xMis = MY_X_MIS;
        hb->yMis = MY_Y_MIS;
    } else {
        hb->xMis = -1;
        hb->yMis = -1;
    }
    sendPacket(hb);
}

/* ----------------------------------------------------------------------- */

/* Packet processing routines */

void processPacket (MWEvent *eventPacket)
{

    MW244BPacket    *pack = eventPacket->eventDetail;
    uint8_t *payload = reinterpret_cast<uint8_t *>(pack);
    size_t size = sizeof(MW244BPacket);
    switch (pack->type) {
    case TYPE_HEARTBEAT: {
        heartbeat pkt;
        pkt.deserialize(payload, size);
        processHeartbeat(&pkt);
    }
    break;
    case TYPE_NAME_REQUEST: {
        nameRequest pkt;
        pkt.deserialize(payload, size);
        std::cout << "name request" << endl;
        processNameRequest(&pkt);
    }
    break;
    case TYPE_NAME_RESPONSE: {
        nameResponse pkt;
        pkt.deserialize(payload, size);
        std::cout << "name response" << endl;
        processNameResponse(&pkt);
    }
    break;
    case TYPE_KILLED: {
        killed pkt;
        pkt.deserialize(payload, size);
        std::cout << "killed" << endl;
        processKilled(&pkt);
    }
    break;
    case TYPE_KILLCONFIRMED: {
        killConfirmed pkt;
        pkt.deserialize(payload, size);
        std::cout << "killConfirmed" << endl;
        processKillConfirmed(&pkt);
    }
    break;
    case TYPE_LEAVE: {
        leave pkt;
        pkt.deserialize(payload, size);
        std::cout << "leave" << endl;
        processLeave(&pkt);
    }
    break;
    default:
        MWError("Unknown incoming protocol");
    }

    //packX = (DataStructureX *) &(pack->body);

}

void processHeartbeat(heartbeat *hb)
{
    if(hb->id == M->myRatId().value()) {
        /* from Me */
        //TODO
    } else {
        bool existing = false;
        int index;
        Rat r;
        for(index = 1; index < MAX_RATS; index++) {
            r = M->rat(index);
            if(r.playing && r.id.value() == hb->id) {
                existing = true;
                break;
            }
            if(!r.playing) {
                break;
            }
        }
        if(existing) {
            /* from existing player */
            if(r.seqNum < hb->seqNum) {
                Rat old = r;
                r.seqNum = hb->seqNum;
                r.x = Loc(hb->xLoc);
                r.y = Loc(hb->yLoc);
                r.dir = Direction(hb->dir);
                r.score = Score(hb->score);
                if((r.hasMissile = hb->hasMissile()) == true) {
                    r.xMis = Loc(hb->xMis);
                    r.yMis = Loc(hb->yMis);
                }
                M->ratIs(r, index);
                if(old.score.value() != r.score.value()) {
                    UpdateScoreCard(index);
                }
            }
        } else {
            /* from new player */
            if(index == MAX_RATS) {
                cout << "Reached max player. New player ignored." << endl;
            } else {
                Rat nr;
                nr.playing = true;
                nr.id = RatId(hb->id);
                nr.seqNum = hb->seqNum;
                nr.x = Loc(hb->xLoc);
                nr.y = Loc(hb->yLoc);
                nr.dir = Direction(hb->dir);
                nr.score = Score(hb->score);
                if((r.hasMissile = hb->hasMissile()) == true) {
                    nr.xMis = Loc(hb->xMis);
                    nr.yMis = Loc(hb->yMis);
                }
                M->ratIs(nr, index);
                printf("new player[%d]=%X\n", index, nr.id.value());
                /* Get name new player name */
                nameRequest *pkt =
                    (nameRequest *)packetFactory::createPacket(TYPE_NAME_REQUEST);
                pkt->id = M->myRatId().value();
                pkt->seqNum = seqNum++;
                pkt->targetId = nr.id.value();
                sendPacket(pkt);
            }
        }
    }

}

void processNameRequest(nameRequest *pkt)
{
    if(pkt->targetId == M->myRatId().value()) {
        /* Respond only when asking my name */
        nameResponse *pkt =
            (nameResponse *)packetFactory::createPacket(TYPE_NAME_RESPONSE);
        pkt->id = M->myRatId().value();
        pkt->seqNum = seqNum++;
        const char *name = GetRatName(MY_RAT_INDEX);
        memcpy(pkt->name, name, strlen(name) + 1);
        sendPacket(pkt);
        printf("[%X]sending res\n", pkt->id);
    }
}

void processNameResponse(nameResponse *pkt)
{
    if(pkt->id == M->myRatId().value()) {
        return;
    }
    printf("[%X]res from %X\n", M->myRatId().value(), pkt->id);
    RatIndexType index = getRatIndexById(RatId(pkt->id));
    if(index == MAX_RATS) {
        MWError("Name response from unknown client.");
    } else {
        Rat r = M->rat(index);
        r.name = string(pkt->name);
        M->ratIs(r, index);
        UpdateScoreCard(index);
    }
}

void processKilled(killed *)
{

}

void processKillConfirmed(killConfirmed *)
{

}
void processLeave(leave *)
{

}
/* ----------------------------------------------------------------------- */

/* This will presumably be modified by you.
   It is here to provide an example of how to open a UDP port.
   You might choose to use a different strategy
 */
void
netInit()
{
    Sockaddr		nullAddr;
    Sockaddr		*thisHost;
    char			buf[128];
    int				reuse;
    u_char          ttl;
    struct ip_mreq  mreq;

    /* MAZEPORT will be assigned by the TA to each team */
    M->mazePortIs(htons(MAZEPORT));

    gethostname(buf, sizeof(buf));
    if ((thisHost = resolveHost(buf)) == (Sockaddr *) NULL)
        MWError("who am I?");
    bcopy((caddr_t) thisHost, (caddr_t) (M->myAddr()), sizeof(Sockaddr));

    M->theSocketIs(socket(AF_INET, SOCK_DGRAM, 0));
    if (M->theSocket() < 0)
        MWError("can't get socket");

    /* SO_REUSEADDR allows more than one binding to the same
       socket - you cannot have more than one player on one
       machine without this */
    reuse = 1;
    if (setsockopt(M->theSocket(), SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse)) < 0) {
        MWError("setsockopt failed (SO_REUSEADDR)");
    }

    nullAddr.sin_family = AF_INET;
    nullAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    nullAddr.sin_port = M->mazePort();
    if (bind(M->theSocket(), (struct sockaddr *)&nullAddr,
             sizeof(nullAddr)) < 0)
        MWError("netInit binding");

    /* Multicast TTL:
       0 restricted to the same host
       1 restricted to the same subnet
       32 restricted to the same site
       64 restricted to the same region
       128 restricted to the same continent
       255 unrestricted

       DO NOT use a value > 32. If possible, use a value of 1 when
       testing.
    */

    ttl = 1;
    if (setsockopt(M->theSocket(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
                   sizeof(ttl)) < 0) {
        MWError("setsockopt failed (IP_MULTICAST_TTL)");
    }

    /* join the multicast group */
    mreq.imr_multiaddr.s_addr = htonl(MAZEGROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(M->theSocket(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)
                   &mreq, sizeof(mreq)) < 0) {
        MWError("setsockopt failed (IP_ADD_MEMBERSHIP)");
    }

    /*
     * Now we can try to find a game to join; if none, start one.
     */

    printf("\n");

    /* set up some stuff strictly for this local sample */
    M->myRatIdIs(0x12345678);
    M->scoreIs(42);
    SetMyRatIndexType(0);

    /* Get the multi-cast address ready to use in SendData()
           calls. */
    memcpy(&groupAddr, &nullAddr, sizeof(Sockaddr));
    groupAddr.sin_addr.s_addr = htonl(MAZEGROUP);

}


/* ----------------------------------------------------------------------- */

void checkJoinComplete(timeval startup)
{
    struct timeval cur;
    gettimeofday(&cur, NULL);
    if (timediff(cur, startup) > JOIN_TIMEOUT) {
        phase = Init;
    }
}

/* generate unique ID given existing rats */
uint32_t generateId()
{
    uint32_t id = 0;
    bool conflict;
    do {
        conflict = false;
        id = (uint32_t)rand();
        for(int i = 1; i < MAX_RATS; i++) {
            Rat r = M->rat(i);
            if(r.playing && r.id.value() == id) {
                conflict = true;
                break;
            }
        }
    } while(conflict);
    return id;
}

long timediff(timeval t1, timeval t2)
{
    return (t1.tv_sec - t2.tv_sec) * 1000 + (t1.tv_usec - t2.tv_usec) / 1000;
}

RatIndexType getRatIndexById(RatId id)
{
    int index;
    Rat r;
    for(index = 1; index < MAX_RATS; index++) {
        r = M->rat(index);
        printf("Target=%X Check:[%d]=%X\n", id.value(), index, r.id.value());
        if(r.playing && r.id.value() == id.value()) {
            break;
        }
    }
    return index;
}

bool isConflictPosition(Loc x, Loc y)
{
    int index;
    Rat r;
    for(index = 1; index < MAX_RATS; index++) {
        r = M->rat(index);
        printf("Check:[%d]=(%d,%d)\n", index, r.x.value(), r.y.value());
        if(r.playing &&
                (r.x.value() == x.value() && r.y.value() == y.value())) {
            return true;
        }
    }
    return false;
}
