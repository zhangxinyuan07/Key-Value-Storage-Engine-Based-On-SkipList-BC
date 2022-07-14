#include <iostream>
#include <unistd.h>
#include "skiplist.h"
#define FILE_PATH "./store/dumpFile"

void printLRUCache(LRU<int, string> *lruCache)
{
    cout << "-------------LRUCache--------------------" << endl;
    for (const auto &p : lruCache->l)
    {
        cout << "key: " << p.first << ", value : " << p.second << endl;
    }
    cout << "-------------LRUCache--------------------" << endl;
}

int main()
{

    // 键值中的key用int型，如果用其他类型，需要自定义比较函数
    // 而且如果修改key的类型，同时需要修改skipList.load_file函数
    // 插入18个元素
    SkipList<int, std::string> skipList(8);
    skipList.insert_element(1, "test1");
    skipList.insert_element(3, "test2");
    skipList.insert_element(7, "test3");
    skipList.insert_element(8, "test4");
    skipList.insert_element(9, "test5");
    skipList.insert_element(19, "test6");
    skipList.insert_element(23, "testaa");
    skipList.insert_element(25, "testbb");
    skipList.insert_element(26, "testcc");
    skipList.insert_element(28, "testdd");
    skipList.insert_element(32, "testee");
    skipList.insert_element(36, "testff");
    skipList.insert_element(41, "testgg");
    skipList.insert_element(51, "testhh");
    skipList.insert_element(53, "testii");
    skipList.insert_element(61, "testjj");
    skipList.insert_element(65, "testkk");
    skipList.insert_element(66, "testll");
    skipList.display_list();
    std::cout << "skipList size:" << skipList.size() << std::endl;

    std::cout << "LRU 缓存大小为： " << skipList.lruCache->capacity << std::endl;
    // 设置11个元素过期时间
    skipList.expire_element(1, 10);
    sleep(1);
    skipList.ttl_element(1);
    sleep(1);
    skipList.expire_element(3, 5);
    sleep(1);
    skipList.ttl_element(3);
    sleep(1);
    skipList.expire_element(7, 5);
    sleep(1);
    skipList.expire_element(9, 6);

    printLRUCache(skipList.lruCache);
    cout << "LRU缓存中元素个数: " << skipList.lruCache->l.size() << endl;

    sleep(1);
    skipList.expire_element(19, 15);
    sleep(1);
    skipList.ttl_element(1);
    skipList.ttl_element(3);

    printLRUCache(skipList.lruCache);
    cout << "LRU缓存中元素个数: " << skipList.lruCache->l.size() << endl;

    skipList.expire_element(25, 30);
    sleep(1);
    skipList.ttl_element(7);

    printLRUCache(skipList.lruCache);
    skipList.display_list();

    skipList.expire_element(26, 7);
    cout << "LRU缓存中元素个数: " << skipList.lruCache->l.size() << endl;
    sleep(1);
    skipList.ttl_element(1);
    skipList.ttl_element(7);
    skipList.expire_element(32, 6);
    sleep(1);
    cout << "LRU缓存中元素个数: " << skipList.lruCache->l.size() << endl;
    skipList.ttl_element(7);
    skipList.expire_element(41, 6);

    printLRUCache(skipList.lruCache);

    skipList.expire_element(51, 5);
    sleep(1);
    skipList.ttl_element(19);
    skipList.expire_element(66, 5);
    sleep(1);

    skipList.display_list();

    std::cout << "skipList size:" << skipList.size() << std::endl;

    skipList.dump_file();

    // skipList.load_file();

    string val;
    if (skipList.search_element(9, &val) == true)
    {
        cout << "查询到key : " << 9 << " value : " << val << endl;
    }
    else
        cout << "未查询到key: " << 9 << endl;

    if (skipList.search_element(18, &val) == true)
    {
        cout << "查询到key : " << 18 << " value : " << val << endl;
    }
    else
        cout << "未查询到key: " << 18 << endl;

    skipList.display_list();

    skipList.delete_element(3);
    skipList.delete_element(7);

    std::cout << "skipList size:" << skipList.size() << std::endl;
    skipList.display_list();
    return 0;
}
