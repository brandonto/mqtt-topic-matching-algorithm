#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define FATAL() exit(1);

typedef std::string SubscriberId;
class Subscription
{
public:
    Subscription(SubscriberId subscriberId, std::string topic)
    {
        subscriberId_m = subscriberId;
        topic_m = topic;
    }

    SubscriberId getSubscriberId()
    {
        return subscriberId_m;
    }

    std::string getTopic()
    {
        return topic_m;
    }

    friend std::ostream& operator<<(
            std::ostream& s,
            const Subscription& req)
    {
        return s << "(" << req.subscriberId_m << ", " << req.topic_m << ")";
    }

private:
    SubscriberId subscriberId_m;
    std::string topic_m;
};

typedef std::vector<Subscription> SubscriptionList;
void dumpSubscriptionList(SubscriptionList& list)
{
    for (auto& req : list)
    {
        std::cout << req << std::endl;
    }
}

class TopicStore
{
public:
    virtual void addTopicSubscription(
            SubscriberId subscriberId,
            const std::vector<std::string>& topicTokens) = 0;

    virtual bool removeTopicSubscription(
            SubscriberId subscriberId,
            const std::vector<std::string>& topicTokens) = 0;

    virtual std::vector<SubscriberId> getSubscriptionMatches(
            const std::vector<std::string>& topicTokens) = 0;
};

class TopicTrie : public TopicStore
{
public:
    class TrieNode
    {
    public:
        typedef std::shared_ptr<TrieNode> TrieNodePtr;
        typedef std::unordered_map<std::string, TrieNodePtr> HashTable;
        typedef HashTable TrieNodes;

        TrieNode()
        {
        }

        TrieNode(std::string topic)
        {
            topic_m = topic;
        }

        ~TrieNode()
        {
            nodes_m.clear();
        }

        bool hasChildNode(std::string topic)
        {
            bool keyExists = (nodes_m.find(topic) != nodes_m.end());
            bool childNodeExists = keyExists && (nodes_m[topic] != nullptr);
            return childNodeExists;
        }

        TrieNodePtr getChildNode(std::string topic)
        {
            return nodes_m[topic];
        }

        bool createChildNode(std::string topic)
        {
            nodes_m[topic] = TrieNodePtr(new TrieNode(topic));
            return true;
        }

        bool deleteChildNode(std::string topic)
        {
            auto it = nodes_m.find(topic);
            if (it != nodes_m.end())
            {
                nodes_m.erase(it);
                return true;
            }

            return false;
        }

        int getNumChildNodes()
        {
            return nodes_m.size();
        }

        std::string getTopic()
        {
            return topic_m;
        }

        void setTopic(std::string topic)
        {
            topic_m = topic;
        }

        void addSubscriber(SubscriberId id)
        {
            subscriberIds_m.insert(id);
        }

        bool removeSubscriber(SubscriberId id)
        {
            auto it = subscriberIds_m.find(id);
            if (it != subscriberIds_m.end())
            {
                subscriberIds_m.erase(it);
                return true;
            }

            return false;
        }

        int getNumSubscribers()
        {
            return subscriberIds_m.size();
        }

        std::set<SubscriberId> getSubscriberIds()
        {
            return subscriberIds_m;
        }

    private:
        std::string topic_m;
        std::set<SubscriberId> subscriberIds_m;
        TrieNodes nodes_m;
    };
    typedef TrieNode::TrieNodePtr TrieNodePtr;

    TopicTrie() :
        rootNode_mp(new TrieNode())
    {
    }

    void addTopicSubscription(
            SubscriberId subscriberId,
            const std::vector<std::string>& topicTokens)
    {
        TrieNodePtr currNode_p = rootNode_mp;
        for (auto it = topicTokens.begin(); it != topicTokens.end(); it++)
        {
            std::string token = *it;

            // Create the child node if one does not exist
            //
            if (!currNode_p->hasChildNode(token))
            {
                if (!currNode_p->createChildNode(token))
                {
                    // Error
                    return;
                }
                //std::cout << "Created node: " << token << std::endl;
            }

            //std::cout << "Walked node: " << token << std::endl;
            currNode_p = currNode_p->getChildNode(token);

            // Add the subscriber ID to the node if this is the last topic token
            //
            if (it == (topicTokens.end()-1))
            {
                //std::cout << "Adding subscriber (" << subscriberId << ") to node (" << currNode_p->getTopic() << ")" << std::endl;
                currNode_p->addSubscriber(subscriberId);
            }
        }
    }

    bool removeTopicSubscription(
            SubscriberId subscriberId,
            const std::vector<std::string>& topicTokens)
    {
        std::vector<std::string>::const_iterator it = topicTokens.begin();
        return walkTrieRemoveSubscription(rootNode_mp, it, topicTokens, subscriberId);
    }

    std::vector<SubscriberId> getSubscriptionMatches(
            const std::vector<std::string>& topicTokens)
    {
        std::vector<SubscriberId> matches;
        std::vector<std::string>::const_iterator it = topicTokens.begin();
        walkTrieGetMatches(rootNode_mp, it, topicTokens, matches);
        return matches;
    }

private:
    void walkTrieGetMatches(
            TrieNodePtr currNode_p,
            std::vector<std::string>::const_iterator it,
            const std::vector<std::string>& topicTokens,
            std::vector<SubscriberId>& matches)
    {
        if (it == topicTokens.end())
        {
            // We've reached the last topic token, add all subscribers of this
            // node to the list of matches
            //
            addNodeSubscriptionsToMatches(currNode_p, matches);

            // This handles the case where there is a multi-level wildcard child
            // node one level past the last topic token. This results in a match
            // because the multi-level wildcard can match zero or more topics.
            //
            if (currNode_p->hasChildNode("#"))
            {
                //std::cout << "Walked node: #" << std::endl;
                TrieNodePtr mlwcNode_p = currNode_p->getChildNode("#");
                addNodeSubscriptionsToMatches(mlwcNode_p, matches);
            }

            return;
        }

        std::string token = *it;

        // If the current node has a multi-level wildcard child node, then all
        // subscribers subscribed to that node matches the current pattern.
        //
        if (currNode_p->hasChildNode("#"))
        {
            //std::cout << "Walked node: #" << std::endl;
            TrieNodePtr mlwcNode_p = currNode_p->getChildNode("#");
            addNodeSubscriptionsToMatches(mlwcNode_p, matches);
        }

        // If the current node has a single-level wildcard child node, things
        // get a little trickier...
        //
        // We want to continue to walk the trie in its own seperate state so we
        // can backtrack to the current state to finish walking the trie.
        //
        if (currNode_p->hasChildNode("+"))
        {
            //std::cout << "Walked node: +" << std::endl;
            TrieNodePtr slwcNode_p = currNode_p->getChildNode("+");
            std::vector<std::string>::const_iterator nextTokenIt = it+1;
            walkTrieGetMatches(slwcNode_p, nextTokenIt, topicTokens, matches);
        }

        // "Normal" cases start here...
        //
        // 1. The current node doesn't have a child node matching the topic
        //    token:
        //
        //   Our walk is complete.
        //
        // 2. The current node has a child node matching the topic token:
        //
        //   Continue walking the trie.
        //
        if (!currNode_p->hasChildNode(token))
        {
            return;
        }

        //std::cout << "Walked node: " << token << std::endl;
        TrieNodePtr nextNode_p = currNode_p->getChildNode(token);

        walkTrieGetMatches(nextNode_p, ++it, topicTokens, matches);
    }

    // Removes a subscription from a node on the trie. This will delete the node
    // (and any of its parent nodes) if there are no other subscriptions to it.
    //
    // i.e. Removing "a/b/d" from the left trie results in the right trie
    //
    //   root         root
    //    |            |
    //    a     =>     a
    //    |\           |
    //    b c          c
    //    |
    //    d
    //
    // Returns true if the subscriber was successfully deleted
    // Returns false otherwise
    // 
    bool walkTrieRemoveSubscription(
            TrieNodePtr currNode_p,
            std::vector<std::string>::const_iterator it,
            const std::vector<std::string>& topicTokens,
            SubscriberId subscriberId)
    {
        if (it == topicTokens.end())
        {
            if (currNode_p->removeSubscriber(subscriberId))
            {
                return true;
            }

            return false;
        }

        std::string token = *it;

        if (!currNode_p->hasChildNode(token))
        {
            return false;
        }

        //std::cout << "Walked node: " << token << std::endl;
        TrieNodePtr nextNode_p = currNode_p->getChildNode(token);
        std::vector<std::string>::const_iterator nextTokenIt = it+1;

        if (!walkTrieRemoveSubscription(nextNode_p, nextTokenIt, topicTokens, subscriberId))
        {
            return false;
        }

        if ((nextNode_p->getNumSubscribers() == 0) && (nextNode_p->getNumChildNodes() == 0))
        {
            if (!currNode_p->deleteChildNode(token))
            {
                // Should never get to this state since we know that the child
                // node exists... fatal
                //
                FATAL();
            }
        }

        return true;
    }

    void addNodeSubscriptionsToMatches(
            TrieNodePtr node_p,
            std::vector<SubscriberId>& matches)
    {
        std::set<SubscriberId> subscriberIds = node_p->getSubscriberIds();
        for (auto id : subscriberIds)
        {
            matches.push_back(id);
            //std::cout << "Match id: " << id << std::endl;
        }
    }

    TrieNodePtr rootNode_mp;
};

class TopicManager
{
public:
    TopicManager() :
        store_mp(new TopicTrie())
    {
    }

    ~TopicManager()
    {
        if (store_mp != nullptr) delete [] store_mp;
    }

    void addSubscription(Subscription sub)
    {
        std::cout << "Adding subscription: " << sub << std::endl;
        std::vector<std::string> topicTokens = tokenizeTopic(sub.getTopic());
        store_mp->addTopicSubscription(sub.getSubscriberId(), topicTokens);
    }

    bool removeSubscription(Subscription sub)
    {
        std::cout << "Removing subscription: " << sub << std::endl;
        std::vector<std::string> topicTokens = tokenizeTopic(sub.getTopic());
        return store_mp->removeTopicSubscription(sub.getSubscriberId(), topicTokens);
    }

    std::vector<SubscriberId> getSubscriptionMatches(std::string topic)
    {
        std::cout << "Getting matches for topic: " << topic << std::endl;
        std::vector<std::string> topicTokens = tokenizeTopic(topic);
        return store_mp->getSubscriptionMatches(topicTokens);
    }

private:
    std::vector<std::string> tokenizeTopic(std::string topic)
    {
        std::vector<std::string> topicTokens;
        std::string item;
        std::stringstream ss(topic);
        while (std::getline(ss, item, '/'))
        {
            topicTokens.push_back(item);
        }

        return topicTokens;
    }

    TopicStore* store_mp;
};

void dumpTopicMatches(const std::vector<std::string> &matches)
{
    std::cout << "Matches: ( ";
    for (auto& match : matches)
    {
        std::cout << match << " ";
    }
    std::cout << ")" << std::endl;
}

int main(int argc, char* argv[])
{
    SubscriptionList subscriptionList;
    TopicManager topicManager;

    // Initialize subscription list
    //
    //subscriptionList.push_back(Subscription("Subscriber01", "a/c/c"));
    //subscriptionList.push_back(Subscription("Subscriber01", "b/b/c"));
    //subscriptionList.push_back(Subscription("Subscriber02", "a/+/b/c"));
    //subscriptionList.push_back(Subscription("Subscriber03", "a/#"));
    //subscriptionList.push_back(Subscription("Subscriber04", "b/b/c"));
    //subscriptionList.push_back(Subscription("Subscriber05", "b/#"));
    //subscriptionList.push_back(Subscription("Subscriber06", "+"));
    //subscriptionList.push_back(Subscription("Subscriber07", "+/+"));
    //subscriptionList.push_back(Subscription("Subscriber08", "+/a"));
    //subscriptionList.push_back(Subscription("Subscriber09", "#"));
    //subscriptionList.push_back(Subscription("Subscriber10", "b/+/c"));
    //subscriptionList.push_back(Subscription("Subscriber11", "+/+/c"));
    //subscriptionList.push_back(Subscription("Subscriber12", "+/#"));
    //subscriptionList.push_back(Subscription("Subscriber13", "b/b/#"));
    //subscriptionList.push_back(Subscription("Subscriber14", "b/b/c/#"));
    //subscriptionList.push_back(Subscription("Subscriber15", "b/b/+"));
    //subscriptionList.push_back(Subscription("Subscriber16", "b/b/c/+"));
    subscriptionList.push_back(Subscription("Subscriber01", "a/b/d"));
    subscriptionList.push_back(Subscription("Subscriber02", "a/c"));

    for (auto &sub : subscriptionList)
    {
        topicManager.addSubscription(sub);
    }

    //dumpTopicMatches(topicManager.getSubscriptionMatches("b/b/c"));

    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b/d"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/c"));

    topicManager.removeSubscription(Subscription("Subscriber01", "a/b/d"));

    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b/d"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/c"));

    topicManager.removeSubscription(Subscription("Subscriber02", "a"));

    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b/d"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/c"));

    topicManager.removeSubscription(Subscription("Subscriber02", "a/c"));

    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b/d"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/b"));
    dumpTopicMatches(topicManager.getSubscriptionMatches("a/c"));

    return 0;
}
