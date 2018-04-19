#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

typedef std::string SubscriberId;
class SubscriptionRequest
{
public:
    SubscriptionRequest(SubscriberId subscriberId, std::string subscription)
    {
        subscriberId_m = subscriberId;
        subscription_m = subscription;
    }

    SubscriberId getSubscriberId()
    {
        return subscriberId_m;
    }

    std::string getSubscription()
    {
        return subscription_m;
    }

    friend std::ostream &operator<<(std::ostream &s, const SubscriptionRequest& req)
    {
        return s << "(" << req.subscriberId_m << ", " << req.subscription_m << ")";
    }

private:
    SubscriberId subscriberId_m;
    std::string subscription_m;
};

typedef std::vector<SubscriptionRequest> SubscriptionList;
SubscriptionList subscriptionList_g;

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
    virtual void addSubscriptionTokens(SubscriberId subscriberId, std::vector<std::string> subscriptionTokens) = 0;
    virtual std::vector<SubscriberId> getSubscriptionMatches(std::vector<std::string> subscriptionTokens) = 0;
};

class TopicTrie : public TopicStore
{
public:
    class TrieNode
    {
    public:
        typedef std::unordered_map<std::string, TrieNode*> HashTable;
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

        TrieNode* getChildNode(std::string topic)
        {
            return nodes_m[topic];
        }

        bool createChildNode(std::string topic)
        {
            nodes_m[topic] = new TrieNode(topic);
            return true;
        }

        std::string getTopic()
        {
            return topic_m;
        }

        void setTopic(std::string topic)
        {
            topic_m = topic;
        }

        void addSubscriberId(SubscriberId id)
        {
            subscriberIds_m.push_back(id);
        }

        std::vector<SubscriberId> getSubscriberIds()
        {
            return subscriberIds_m;
        }

    private:
        std::string topic_m;
        std::vector<SubscriberId> subscriberIds_m;
        TrieNodes nodes_m;
    };

    TopicTrie()
    {
    }

    void addSubscriptionTokens(SubscriberId subscriberId, std::vector<std::string> subscriptionTokens)
    {
        TrieNode* currNode_p = &root_m;
        for (auto it = subscriptionTokens.begin(); it != subscriptionTokens.end(); it++)
        {
            std::string token = *it;
            if (currNode_p->hasChildNode(token))
            {
                std::cout << "Walked node: " << token << std::endl;
                currNode_p = currNode_p->getChildNode(token);
            }
            else
            {
                if (!currNode_p->createChildNode(token))
                {
                    // Error
                    return;
                }
                std::cout << "Created node: " << token << std::endl;
                currNode_p = currNode_p->getChildNode(token);
            }

            // Mark the node with the subscriber ID if this is the last token in the subscription string
            //
            if (it == (subscriptionTokens.end()-1))
            {
                std::cout << "Adding subscriber (" << subscriberId << ") to node (" << currNode_p->getTopic() << ")" << std::endl;
                currNode_p->addSubscriberId(subscriberId);
            }
        }
    }

    // TODO: Handle '+' and '#' tokens correctly
    //
    std::vector<SubscriberId> getSubscriptionMatches(std::vector<std::string> subscriptionTokens)
    {
        std::vector<SubscriberId> matches;
        TrieNode* currNode_p = &root_m;
        for (auto it = subscriptionTokens.begin(); it != subscriptionTokens.end(); it++)
        {
            std::string token = *it;

            if (!currNode_p->hasChildNode(token))
            {
                break;
            }

            std::cout << "Walked node: " << token << std::endl;
            currNode_p = currNode_p->getChildNode(token);

            // If this is the last token in the subscription string, add all subscribers of this node to the list of matches
            //
            if (it == (subscriptionTokens.end()-1))
            {
                std::vector<SubscriberId> subscriberIds = currNode_p->getSubscriberIds();
                for (auto id : subscriberIds)
                {
                    matches.push_back(id);
                }
            }
        }
        return matches;
    }

private:
    TrieNode root_m;
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

    void addSubscription(SubscriptionRequest& req)
    {
        std::cout << "Adding subscription: " << req << std::endl;
        std::vector<std::string> subscriptionTokens = tokenizeSubscription(req.getSubscription());
        store_mp->addSubscriptionTokens(req.getSubscriberId(), subscriptionTokens);
    }

    void addSubscriptionList(SubscriptionList& list)
    {
        for (auto& req : list)
        {
            addSubscription(req);
        }
    }

    std::vector<SubscriberId> getSubscriptionMatches(std::string subscription)
    {
        std::vector<std::string> subscriptionTokens = tokenizeSubscription(subscription);
        return store_mp->getSubscriptionMatches(subscriptionTokens);
    }

private:
    std::vector<std::string> tokenizeSubscription(std::string subscription)
    {
        std::vector<std::string> subscriptionTokens;
        std::string item;
        std::stringstream ss(subscription);
        while (std::getline(ss, item, '/'))
        {
            subscriptionTokens.push_back(item);
        }

        return subscriptionTokens;
    }

    TopicStore* store_mp;
};
TopicManager topicManager_g;

int main(int argc, char* argv[])
{
    // Initialize subscription list
    //
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber1", "a/c/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber1", "b/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber2", "a/+/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber3", "a/#/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber4", "b/b/c"));
    subscriptionList_g.push_back(SubscriptionRequest("Subcriber5", "b/#"));

    topicManager_g.addSubscriptionList(subscriptionList_g);

    std::string publishedTopic = "b/b/c";
    std::vector<std::string> matches = topicManager_g.getSubscriptionMatches(publishedTopic);
    std::cout << "Matches: ( ";
    for (auto& match : matches)
    {
        std::cout << match << " ";
    }
    std::cout << ")" << std::endl;

    return 0;
}
