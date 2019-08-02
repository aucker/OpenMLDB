package com._4paradigm.rtidb.client.ha;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com._4paradigm.rtidb.client.ha.TableHandler.ReadStrategy;

import rtidb.api.TabletServer;

public class PartitionHandler {
    private final static Logger logger = LoggerFactory.getLogger(PartitionHandler.class);
    private final static Random rand = new Random(System.currentTimeMillis());
    private TabletServer leader = null;
    private List<TabletServer> followers = new ArrayList<TabletServer>();
    // the fast server for tablet read
    private TabletServer fastTablet = null;
    private List<TabletServer> tabletServerList = new ArrayList<TabletServer>();

    public TabletServer getLeader() {
        return leader;
    }

    public void setLeader(TabletServer leader) {
        this.leader = leader;
    }

    public List<TabletServer> getFollowers() {
        return followers;
    }

    public void setFollowers(List<TabletServer> followers) {
        this.followers = followers;
    }

    public TabletServer getFastTablet() {
        return fastTablet;
    }

    public void setFastTablet(TabletServer fastTablet) {
        this.fastTablet = fastTablet;
    }

    public TabletServer getReadHandler(ReadStrategy strategy) {
        // single node tablet
        if (followers.size() <= 0 || strategy == null) {
            return leader;
        }
        switch (strategy) {
            case kReadLeader:
                logger.debug("choose leader partition for reading");
                return leader;
            case kReadFollower:
                if (followers.size() > 0) {
                    logger.debug("rand choose follower partition for reading");
                    int index = rand.nextInt(followers.size());
                    return followers.get(index);
                } else {
                    logger.debug("choose leader partition for reading");
                    return leader;
                }
            case kReadLocal:
                if (fastTablet != null) {
                    logger.debug("choose fast partition for reading");
                    return fastTablet;
                } else if (followers.size() > 0) {
                    logger.debug("rand choose follower partition for reading");
                    int index = rand.nextInt(followers.size());
                    return followers.get(index);
                } else {
                    return leader;
                }
            case KReadRandom:
                tabletServerList.clear();
                logger.debug("rand choose partition for reading");
                if (leader != null) {
                    tabletServerList.add(leader);
                }
                if (followers.size() > 0) {
                    for (int i = 0; i < followers.size(); i++) {
                        tabletServerList.add(followers.get(i));
                    }
                }
                if (tabletServerList.size() > 0) {
                    int index = rand.nextInt(tabletServerList.size());
                    return tabletServerList.get(index);
                } else {
                    logger.error("no available partition for reading");
                    return null;
                }
            default:
                return leader;
        }
    }


}
