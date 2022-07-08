#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <iostream> 
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <fstream>

#define STORE_FILE "store/dumpFile"

std::mutex mtx;     // 修改跳表时需要加锁
std::string delimiter = ":";

// 跳表中的节点类
template<typename K, typename V> 
class Node {

public:
    
    Node() {} 

    Node(K k, V v, int); 

    ~Node();
    
    // 成员函数后加const表示传入的this指针为const指针,该函数不会对该类的(非静态)成员变量作任何改变
    K get_key() const; 

    V get_value() const;

    void set_value(V);

    // 括号里的 * 表示forward是一个指针
    // 括号外的 Node<K, V>* 表示forward指向的数据类型
    // forward用于指向一个指针数组,这个数组里保存了指向跳表当前层的下一层所有节点的指针
    Node<K, V> *(*forward);

    int node_level;

private:
    K key;
    V value;
};

template<typename K, typename V> 
Node<K, V>::Node(const K k, const V v, int level) {
    this->key = k;
    this->value = v;
    this->node_level = level;

    // forward指向一个大小为level+1的指针数组
    this->forward = new Node<K, V>*[level+1]; 
    
	// 数组元素以0(NULL)初始化
    memset(this->forward, 0, sizeof(Node<K, V>*)*(level+1));
};

template<typename K, typename V> 
Node<K, V>::~Node() {
    delete []forward;
};

template<typename K, typename V> 
K Node<K, V>::get_key() const {
    return key;
};

template<typename K, typename V> 
V Node<K, V>::get_value() const {
    return value;
};
template<typename K, typename V> 
void Node<K, V>::set_value(V value) {
    this->value=value;
};



/*---------------------------------------------------------------------------------*/



// skiplist类
template <typename K, typename V> 
class SkipList {

public: 
    SkipList(int);
    ~SkipList();
    int get_random_level();
    Node<K, V>* create_node(K, V, int);
    int insert_element(K, V);
    void display_list();
    bool search_element(K);
    void delete_element(K);
    void dump_file();
    void load_file();
    int size();

private:
    void get_key_value_from_string(const std::string& str, std::string* key, std::string* value);
    bool is_valid_string(const std::string& str);

private:    
    // 跳表的最大层数
    int _max_level;

    // 跳表当前所在的层数, 构造函数会初始化为0
    int _skip_list_level;

    // 跳表头节点指针
    Node<K, V> *_header;

    // 跳表当前元素个数, 构造函数会初始化为0
    int _element_count;

    // 文件描述符
    std::ofstream _file_writer;
    std::ifstream _file_reader;
};

// 创建一个新的节点
template<typename K, typename V>
Node<K, V>* SkipList<K, V>::create_node(const K k, const V v, int level) {
    Node<K, V> *n = new Node<K, V>(k, v, level);
    return n;
}

// insert_element()方法用于向跳表插入给定的key和value
// 返回1代表元素存在 
// 返回0代表插入成功
/* 
                           +------------+
                           |  insert 50 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |                      insert +----+
level 3         1+-------->10+---------------> | 50 |          70       100
                                               |    |
                                               |    |
level 2         1          10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 1         1    4     10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 0         1    4   9 10         30   40  | 50 |  60      70       100
                                               +----+

*/
template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value) {
    
    mtx.lock();

    // current指针指向跳表头节点, 接下来将使用current指针来遍历跳表
    Node<K, V> *current = this->_header; 

    // 创建一个update数组,并初始化
    // update数组里放的是node->forward[i]里等待被操作的那些节点
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));  

    // 从跳表的最左上角节点开始查找
    for(int i = _skip_list_level; i >= 0; i--) {
        while(current->forward[i] != NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i]; 
        }
        update[i] = current;
    }

    // 遍历至第0层, 当前指针current指向第一个大于等于待插入节点值的节点
    current = current->forward[0];

    // 如果当前节点的key值和待插入节点key相等，则说明待插入节点值存在。修改该节点的值。
    if (current != NULL && current->get_key() == key) {
        std::cout << "key: " << key << ", exists" << std::endl;
        mtx.unlock();
        return 1;
    }

    // 如果current节点为null 这就意味着要将该元素插入最后一个节点。
    // 如果current的key值和待插入的key不等，代表我们应该在update[0]和current之间插入该节点。
    if (current == NULL || current->get_key() != key ) {

        // 获取当前节点应该建立的随机索引等级
        int random_level = get_random_level();

        // 如果随机索引等级大于跳表当前层级, 用指向头节点的指针初始化 update 中的值
        if (random_level > _skip_list_level) {
            for (int i = _skip_list_level+1; i < random_level+1; i++) {
                update[i] = _header;
            }
            _skip_list_level = random_level;
        }

        // 使用生成的随机索引等级创建新的节点
        Node<K, V>* inserted_node = create_node(key, value, random_level);
        
        // 插入节点
        for (int i = 0; i <= random_level; i++) {
            inserted_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = inserted_node;
        }
        std::cout << "Successfully inserted key:" << key << ", value:" << value << std::endl;
        _element_count ++;
    }
    mtx.unlock();
    return 0;
}

// 显示跳表
template<typename K, typename V> 
void SkipList<K, V>::display_list() {

    std::cout << "\n*****Skip List*****"<<"\n"; 
    for (int i = 0; i <= _skip_list_level; i++) {
        Node<K, V> *node = this->_header->forward[i]; 
        std::cout << "Level " << i << ": ";
        while (node != NULL) {
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

// 将内存中的数据转储到文件中
template<typename K, typename V> 
void SkipList<K, V>::dump_file() {

    std::cout << "dump_file-----------------" << std::endl;
    _file_writer.open(STORE_FILE);
    Node<K, V> *node = this->_header->forward[0]; 

    while (node != NULL) {
        _file_writer << node->get_key() << ":" << node->get_value() << "\n";
        std::cout << node->get_key() << ":" << node->get_value() << ";\n";
        node = node->forward[0];
    }

    _file_writer.flush();
    _file_writer.close();
    return ;
}

// 从磁盘加载数据
template<typename K, typename V> 
void SkipList<K, V>::load_file() {

    _file_reader.open(STORE_FILE);
    std::cout << "load_file-----------------" << std::endl;
    std::string line;
    std::string* key = new std::string();
    std::string* value = new std::string();
    while (getline(_file_reader, line)) {
        get_key_value_from_string(line, key, value);
        if (key->empty() || value->empty()) {
            continue;
        }
        insert_element(*key, *value);
        std::cout << "key:" << *key << "value:" << *value << std::endl;
    }
    _file_reader.close();
}

// 获取当前的 SkipList 大小
template<typename K, typename V> 
int SkipList<K, V>::size() { 
    return _element_count;
}

// 从输入的"key:value"格式的键值对中提取出key和value
template<typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string& str, std::string* key, std::string* value) {

    if(!is_valid_string(str)) {
        return;
    }
    *key = str.substr(0, str.find(delimiter));
    *value = str.substr(str.find(delimiter)+1, str.length());
}

// 判断输入的字符串格式是否合法
template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {

    if (str.empty()) {
        return false;
    }
    if (str.find(delimiter) == std::string::npos) {
        return false;
    }
    return true;
}

// 从跳表中删除元素
template<typename K, typename V> 
void SkipList<K, V>::delete_element(K key) {

    mtx.lock();
    Node<K, V> *current = this->_header; 
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));

    // 从跳表最高层开始遍历
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] !=NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    // current现在指向要删除的节点
    current = current->forward[0];
    if (current != NULL && current->get_key() == key) {
       
        // 从最底层开始,删除每一层要删除的current节点
        for (int i = 0; i <= _skip_list_level; i++) {

            // 如果下个节点不是目标节点了,退出循环
            if (update[i]->forward[i] != current) 
                break;

            update[i]->forward[i] = current->forward[i];
        }

        // 删除没有元素的索引层
        while (_skip_list_level > 0 && _header->forward[_skip_list_level] == 0) {
            _skip_list_level --; 
        }

        std::cout << "Successfully deleted key "<< key << std::endl;
        _element_count --;
    }
    mtx.unlock();
    return;
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
template<typename K, typename V> 
bool SkipList<K, V>::search_element(K key) {

    std::cout << "search_element-----------------" << std::endl;
    Node<K, V> *current = _header;

    // 从跳表左上角开始查找
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }

    // 遍历至第0层, 当前指针current指向第一个大于等于待插入节点值的节点
    current = current->forward[0];

    // 如果当前节点的key等于要查找的key, 则返回
    if (current and current->get_key() == key) {
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }

    std::cout << "Not Found Key:" << key << std::endl;
    return false;
}

// 跳表构造函数
template<typename K, typename V> 
SkipList<K, V>::SkipList(int max_level) {

    this->_max_level = max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;

    // create header node and initialize key and value to null
    K k;
    V v;
    this->_header = new Node<K, V>(k, v, _max_level);
};

// 跳表析构函数
template<typename K, typename V> 
SkipList<K, V>::~SkipList() {

    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
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
template<typename K, typename V>
int SkipList<K, V>::get_random_level(){

    int k = 1;
    // k变成2的概率是1/2,变成2以后再变成3的概率是1/2 * 1/2 = 1/4...
    while (rand() % 2) {
        k++;
    }
    k = (k < _max_level) ? k : _max_level;
    return k;
};

#endif