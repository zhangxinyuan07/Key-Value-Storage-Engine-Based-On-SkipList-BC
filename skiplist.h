#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <fstream>
#include <list>
#include <unordered_map>
#include <time.h>
using namespace std;

#define STORE_FILE "store/dumpFile"

int VOLATILE_LRU_THRESHOLD = 8;

mutex mtx; // 修改跳表时需要加锁
string delimiter = ":";

// LRU缓存类
template <typename K, typename V>
class LRU
{
public:
    int capacity;
    list<pair<K, V>> l;
    unordered_map<K, typename list<pair<K, V>>::iterator> m;

public:
    LRU(int c) : capacity(c){};
    bool get(K, V *valptr = nullptr);
    int put(K, V, K *delKeyPtr = nullptr);
    void del(K);
};

// 获取数据，并将该数据放到 list 头部
template <typename K, typename V>
bool LRU<K, V>::get(K key, V *valptr)
{
    if (m.find(key) != m.end())
    {
        pair<K, V> p = *(m[key]);
        l.erase(m[key]);
        l.push_front(p);
        m[key] = l.begin();
        *valptr = p.second;
        return true;
    }
    else
    {
        return false;
    }
}

// 插入数据，如果 key 已存在，则更新，否则插入。并将它放置到 list 头部
// 如果 list 中数据满了，移除其尾部数据。
// 返回值为1代表正常插入数据, 没有出现溢出而删除元素的情况
// 返回值为0代表元素已存在, 修改了其值
// 返回值为-1代表有溢出, 先删除了一个元素, 那么此时这个元素也要相应地在跳表里被删除
// 要删除的元素通过delKeyPtr传出去删
template <typename K, typename V>
int LRU<K, V>::put(K key, V value, K *delKeyPtr)
{
    if (m.find(key) != m.end())
    {
        l.erase(m[key]);
        l.push_front({key, value});
        m[key] = l.begin();
        return 0;
    }
    else
    {
        if (l.size() >= capacity)
        {
            auto last = l.back();
            l.pop_back();
            m.erase(last.first);
            *delKeyPtr = last.first;
            l.push_front(make_pair(key, value));
            m.insert({key, l.begin()});
            return -1;
        }
        else
        {
            l.push_front({key, value});
            m.insert({key, l.begin()});
            return 1;
        }
    }
}

template <typename K, typename V>
void LRU<K, V>::del(K key)
{
    l.erase(m[key]);
    m.erase(key);
}

/*---------------------------------------------------------------------------------*/

// 跳表中的节点类
template <typename K, typename V>
class Node
{

public:
    Node() {}

    Node(K k, V v, int);

    ~Node();

    // 成员函数后加const表示传入的this指针为const指针
    // 该函数不会对该类的(非静态)成员变量作任何改变
    K get_key() const;

    V get_value() const;

    void set_value(V);

    // 括号里的 * 表示next是一个指针
    // 括号外的 Node<K, V>* 表示next指向的数据类型
    // 跳表的每一层是一个特殊的链表,这个链表的每个节点可能有一个或多个next指针
    // 这里next用于指向一个指针数组, 数组里都是Node节点的指针
    Node<K, V> *(*next);

    int node_level;

private:
    K key;
    V value;
};

// 跳表节点的构造函数
// 每次使用函数构造一个节点之前, 要先通过调用SkipList<K, V>::get_random_level()方法
// 以得知应该为该节点建立几级索引, 级数就通过level参数传入
template <typename K, typename V>
Node<K, V>::Node(const K k, const V v, int level)
{
    this->key = k;
    this->value = v;
    this->node_level = level;

    // next指向一个大小为level+1的指针数组
    // this->next[i]代表当前节点在第i层的下一个节点
    // 也就是说, 如果一个节点在跳表的每一层都出现的话
    // 那么这个节点在每一层都应该有一个next指针,指向该层链表的下一个节点
    // 每个节点应该建立的索引级数为传入的level参数
    // 所以这个节点的next数组大小自然就是level + 1
    this->next = new Node<K, V> *[level + 1];

    // 数组元素以0(NULL)初始化
    memset(this->next, 0, sizeof(Node<K, V> *) * (level + 1));
};

template <typename K, typename V>
Node<K, V>::~Node()
{
    delete[] next;
};

template <typename K, typename V>
K Node<K, V>::get_key() const
{
    return key;
};

template <typename K, typename V>
V Node<K, V>::get_value() const
{
    return value;
};
template <typename K, typename V>
void Node<K, V>::set_value(V value)
{
    this->value = value;
};

/*---------------------------------------------------------------------------------*/

// skiplist类
template <typename K, typename V>
class SkipList
{

public:
    SkipList(int);
    ~SkipList();
    int get_random_level();
    Node<K, V> *create_node(K, V, int);
    int insert_element(K, V);
    void display_list();
    bool search_element(K, V *valptr = nullptr);
    bool delete_element(K);
    void expire_element(K, int);
    int ttl_element(K);
    int isExpire(K);
    void dump_file();
    void load_file();
    int size();

private:
    void get_key_value_from_string(const string &str, string *key, string *value);
    bool is_valid_string(const string &str);

private:
    // 跳表的最大层数
    int _max_level;

    // 跳表当前所在的层数, 构造函数会初始化为0
    int _skip_list_level;

    // 跳表头节点指针
    Node<K, V> *_header;

    // 跳表当前元素个数, 构造函数会初始化为0
    int _element_count;

    // 用于存放设置了过期时间的key对应的时间, pair的第一项为过期时间, pair的第二项为设置时的时间
    // 比如: pair第二项为1000秒, 过期时间为5秒, 则获取系统时间得到的值大于1005秒的时候就过期了
    unordered_map<K, pair<int, time_t>> expire_key_mp;

    // 文件描述符, 用于将数据存盘到本地
    ofstream _file_writer;
    ifstream _file_reader;

public:
    // 设置了过期时间键值对的LRU缓存
    LRU<K, V> *lruCache;
};

// 创建一个新的节点
template <typename K, typename V>
Node<K, V> *SkipList<K, V>::create_node(const K k, const V v, int level)
{
    Node<K, V> *n = new Node<K, V>(k, v, level);
    return n;
}

/*
insert_element()方法用于向跳表插入给定的key和value
返回1代表元素存在
返回0代表插入成功
                           +------------+
                           |  insert 50 |
                           +------------+
level_4     +-->1+                                                      100
                 |
                 |                       insert +----+
level_3         1+-------->10+                  | 50 |          70       100
                             |                  |    |
                             |                  |    |
level_2         1          10+------->30+       | 50 |          70       100
                                        |       |    |
                                        |       |    |
level_1         1    4     10         30+       | 50 |          70       100
                                        |       |    |
                                        |       |    |
level_0         1    4   9 10         30+-->40--| 50 |-->60     70       100
                                                +----+

*/
template <typename K, typename V>
int SkipList<K, V>::insert_element(K key, const V value)
{
    // 有两种清理过期key的方法:
    // 1. 被动清理 : 主动访问一个过期key时, 删除该key
    // 2. 内存不足时触发主动清理 : 在设置了过期时间的键空间中，移除最近最少使用的key

    mtx.lock();
    // 如果过期先指行被动清理原来的, 没过期就先更新LRU里的值
    if (isExpire(key) == 1)
    {
        lruCache->del(key);
        expire_key_mp.erase(key);
        delete_element(key);
        _element_count--;
    }
    else if (isExpire(key) == 0)
    {
        lruCache->put(key, value);
    }

    // current指针指向跳表头节点, 接下来将使用current指针来遍历跳表
    Node<K, V> *current = this->_header;

    // 创建一个update数组,并初始化
    // update数组里放的是node->next[i]里等待被操作的那些节点
    Node<K, V> *update[_max_level + 1];
    memset(update, 0, sizeof(Node<K, V> *) * (_max_level + 1));

    // 从跳表的最左上角节点开始查找
    // current一开始指向level_4的1这个节点, i一开始等于4
    // 那么该节点的->next[4] 就是该节点在level_4这一层的下一个节点, 为空
    // 为空的话就加入update数组, 通过i--这个操作, 就进入了下一层
    for (int i = _skip_list_level; i >= 0; i--)
    {
        while (current->next[i] != NULL && current->next[i]->get_key() < key)
        {
            current = current->next[i];
        }
        // level_4的1, level_3的10, level_2的30, level_1的30, level_0的40
        // 这些节点都在这个for循环里都会被依次加入update数组,以备后续之需
        // 比如要插入的节点50的索引等级为3,
        // 那level_3的10, level_2的30, level_1的30, level_0的40
        // 这几个节点后面都要插入key为50的这个节点, 所以要在遍历过程中把它们都保存到update数组里
        update[i] = current;
    }
    // 退出这个for循环时, current指向的是level_0的40

    // 遍历至第0层, 当前指针current指向第一个大于等于待插入节点值的节点
    current = current->next[0];
    // 此时current指向level_0的60

    // 如果当前节点的key值和待插入节点key相等，则说明待插入节点值存在。
    if (current != NULL && current->get_key() == key)
    {
        cout << "key: " << key << ", exists" << endl;
        current->set_value(value); // 更新其值
        mtx.unlock();
        return 1;
    }

    // 如果current节点为null 这就意味着要将该元素插入最后一个节点。
    // 如果current的key值和待插入的key不等，代表我们应该在update[0]和current之间插入该节点。
    if (current == NULL || current->get_key() != key)
    {

        // 获取当前节点应该建立的随机索引等级
        int random_level = get_random_level();

        // 如果随机索引等级大于跳表当前层级, 用指向头节点的指针初始化 update 中的值
        if (random_level > _skip_list_level)
        {
            for (int i = _skip_list_level + 1; i < random_level + 1; i++)
            {
                update[i] = _header;
            }
            _skip_list_level = random_level;
        }

        // 使用生成的随机索引等级创建新的节点
        Node<K, V> *inserted_node = create_node(key, value, random_level);

        // 插入节点
        // 这个过程如下:
        // level_0 : 节点50的next[0]指向60, update[0]也就是节点40的next[0]指向50
        // level_1 : 节点50的next[1]指向70, update[1]也就是节点30的next[1]指向50
        // level_2 : 节点50的next[2]指向70, update[2]也就是节点30的next[2]指向50
        // level_3 : 节点50的next[3]指向70, update[3]也就是节点10的next[3]指向50
        for (int i = 0; i <= random_level; i++)
        {
            inserted_node->next[i] = update[i]->next[i];
            update[i]->next[i] = inserted_node;
        }
        cout << "Successfully inserted key:" << key << ", value:" << value << endl;
        _element_count++;
    }
    mtx.unlock();
    return 0;
}

// 设置key的过期时间为seconds,单位为秒
template <typename K, typename V>
void SkipList<K, V>::expire_element(K key, int seconds)
{

    V val;
    if (search_element(key, &val) == false)
    {
        cout << "该key不存在, 设置过期时间失败." << endl;
        return;
    }

    time_t timep;
    time(&timep); // 获取从1970至今过了多少秒，存入time_t类型的timep
    expire_key_mp[key] = make_pair(seconds, timep);

    K delKey;
    if (lruCache->put(key, val, &delKey) == -1)
    {
        // 如果LRU里有删除元素,那跳表里也要相应删除元素
        delete_element(delKey);
        cout << "LRU缓存已满, 已自动清理key: " << delKey << endl;
    }
    cout << "成功设置key: " << key << "过期时间为: " << seconds << " 秒!" << endl;
}

// 判断key是否过期, 过期返回1, 否则返回0; 返回-1代表key是永久元素
template <typename K, typename V>
int SkipList<K, V>::isExpire(const K key)
{
    if (expire_key_mp.count(key) == 0)
        return -1;

    time_t timep;
    time(&timep); // 获取从1970至今过了多少秒，存入time_t类型的timep
    if (timep - expire_key_mp[key].second > expire_key_mp[key].first)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// 返回key的剩余时间, 若key为永久的, 返回-1; 若key已过期, 返回0; 若key不存在,返回-2
template <typename K, typename V>
int SkipList<K, V>::ttl_element(const K key)
{
    if (expire_key_mp.count(key) == 0)
        return -1;

    if (isExpire(key) == 1)
    {
        lruCache->del(key);
        delete_element(key);
        _element_count--;
        cout << "key: " << key << " 已过期, 已清理" << endl;
        return 0;
    }

    time_t timep;
    time(&timep); // 获取从1970至今过了多少秒，存入time_t类型的timep
    int sec = expire_key_mp[key].first - (timep - expire_key_mp[key].second);
    cout << "key : " << key << "还有 " << sec << " 秒过期." << endl;
    return sec;
}

// 显示跳表
template <typename K, typename V>
void SkipList<K, V>::display_list()
{

    cout << "-------------------------------SkipList--------------------------------" << endl;
    for (int i = 0; i <= _skip_list_level; i++)
    {
        Node<K, V> *node = this->_header->next[i];
        cout << "Level " << i << ": ";
        while (node != NULL)
        {
            cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->next[i];
        }
        cout << endl;
    }
    cout << "-------------------------------SkipList---------------------------------" << endl;
}

// 将内存中的数据转储到文件中
template <typename K, typename V>
void SkipList<K, V>::dump_file()
{

    cout << "dump_file-----------------" << endl;
    _file_writer.open(STORE_FILE);
    Node<K, V> *node = this->_header->next[0];

    while (node != NULL)
    {
        _file_writer << node->get_key() << ":" << node->get_value() << "\n";
        cout << node->get_key() << ":" << node->get_value() << ";\n";
        node = node->next[0];
    }

    _file_writer.flush();
    _file_writer.close();
    return;
}

// 从磁盘加载数据
template <typename K, typename V>
void SkipList<K, V>::load_file()
{

    _file_reader.open(STORE_FILE);
    cout << "load_file-----------------" << endl;
    string line;
    string *key = new string();
    string *value = new string();
    while (getline(_file_reader, line))
    {
        get_key_value_from_string(line, key, value);
        if (key->empty() || value->empty())
        {
            continue;
        }
        insert_element(*key, *value);
        cout << "key:" << *key << "value:" << *value << endl;
    }
    _file_reader.close();
}

// 获取当前的 SkipList 大小
template <typename K, typename V>
int SkipList<K, V>::size()
{
    return _element_count;
}

// 从输入的"key:value"格式的键值对中提取出key和value
template <typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const string &str, string *key, string *value)
{

    if (!is_valid_string(str))
    {
        return;
    }
    *key = str.substr(0, str.find(delimiter));
    *value = str.substr(str.find(delimiter) + 1, str.length());
}

// 判断输入的字符串格式是否合法
template <typename K, typename V>
bool SkipList<K, V>::is_valid_string(const string &str)
{

    if (str.empty())
    {
        return false;
    }
    if (str.find(delimiter) == string::npos)
    {
        return false;
    }
    return true;
}

// 从跳表中删除元素
template <typename K, typename V>
bool SkipList<K, V>::delete_element(K key)
{
    mtx.lock();

    // LRU里有就先删了
    if (lruCache->m.find(key) != lruCache->m.end())
    {
        lruCache->del(key);
        expire_key_mp.erase(key);
    }

    Node<K, V> *current = this->_header;
    Node<K, V> *update[_max_level + 1];
    memset(update, 0, sizeof(Node<K, V> *) * (_max_level + 1));

    // 从跳表最高层开始遍历
    for (int i = _skip_list_level; i >= 0; i--)
    {
        while (current->next[i] != NULL && current->next[i]->get_key() < key)
        {
            current = current->next[i];
        }
        update[i] = current;
    }

    // current现在指向要删除的节点
    current = current->next[0];
    if (current != NULL && current->get_key() == key)
    {

        // 从最底层开始,删除每一层要删除的current节点
        for (int i = 0; i <= _skip_list_level; i++)
        {

            // 如果下个节点不是目标节点了,退出循环
            if (update[i]->next[i] != current)
                break;

            update[i]->next[i] = current->next[i];
        }

        // 删除没有元素的索引层
        while (_skip_list_level > 0 && _header->next[_skip_list_level] == 0)
        {
            _skip_list_level--;
        }

        // cout << "Successfully deleted key " << key << endl;
        _element_count--;
    }
    mtx.unlock();
    return true;
}

// Search for element in skip list
/*
                           +------------+
                           |  select 60 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |
level 3         1+-------->10+------------------>50+           70       100
                                                   |
                                                   |
level 2         1          10         30         50|           70       100
                                                   |
                                                   |
level 1         1    4     10         30         50|           70       100
                                                   |
                                                   |
level 0         1    4   9 10         30   40    50+-->60      70       100
*/
template <typename K, typename V>
bool SkipList<K, V>::search_element(K key, V *valptr)
{

    // cout << "search_element-----------------" << endl;

    // 先在LRU里找
    V val;
    if (lruCache->get(key, &val) == true)
    {
        *valptr = val;
        return true;
    }

    Node<K, V> *current = _header;

    // 从跳表左上角开始查找
    for (int i = _skip_list_level; i >= 0; i--)
    {
        while (current->next[i] && current->next[i]->get_key() < key)
        {
            current = current->next[i];
        }
    }

    // 遍历至第0层, 当前指针current指向第一个大于等于待插入节点值的节点
    current = current->next[0];

    // 如果当前节点的key等于要查找的key, 则返回其值
    if (current and current->get_key() == key)
    {
        // cout << "Found key: " << key << ", value: " << current->get_value() << endl;
        *valptr = current->get_value();
        return true;
    }

    // cout << "Not Found Key:" << key << endl;
    return false;
}

// 跳表构造函数
template <typename K, typename V>
SkipList<K, V>::SkipList(int max_level)
{

    this->_max_level = max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;

    // create header node and initialize key and value to null
    K k;
    V v;
    this->_header = new Node<K, V>(k, v, _max_level);
    this->lruCache = new LRU<K, V>(VOLATILE_LRU_THRESHOLD);
};

// 跳表析构函数
template <typename K, typename V>
SkipList<K, V>::~SkipList()
{

    if (_file_writer.is_open())
    {
        _file_writer.close();
    }
    if (_file_reader.is_open())
    {
        _file_reader.close();
    }
    delete _header;
}

// 一直向跳表中添加数据,但是不更新索引.就可能出现两个节点中数据过多的情况,跳表会退化为单链表
// 为了高效地更新索引,每当有数据加入时,尽量让该数据有1/2的概率建立一级索引,1/4的概率建立二级索引,1/8的概率建立三级索引...
// 因此需要一个函数,每当有数据插入时,先用该函数的算法告诉我们这个数据需要建立几级索引

// get_random_level()方法会随机返回1到_max_level之间的整数:
// 返回1(概率1/2), 表示当前插入的元素不需要建立索引
// 返回2(概率1/4), 表示当前插入的元素需要建立一级索引
// 返回3(概率1/8), 表示当前插入的元素需要建立二级索引
// 返回4(概率1/16), 表示当前插入的元素需要建立三级索引

// 需要说明的是,一个元素建立二级索引意味着它也要同时建立一级索引,
// 建立三级索引意味着也要同时建立一级和二级索引...
// 因此对于get_random_level()方法来说:
// 返回值大于1就会建立一级索引,概率为 1 - 1/2 = 1/2
// 返回值大于2就会建立二级索引,概率为 1 - 1/2 - 1/4 = 1/4
// 返回值大于3就会建立三级索引,概率为 1 - 1/2 - 1/4 - 1/8 = 1/8
template <typename K, typename V>
int SkipList<K, V>::get_random_level()
{

    int k = 1;
    // k变成2的概率是1/2,变成2以后再变成3的概率是1/2 * 1/2 = 1/4...
    while (rand() % 2)
    {
        k++;
    }
    k = (k < _max_level) ? k : _max_level;
    return k;
};

#endif