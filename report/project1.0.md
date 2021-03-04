تمرین گروهی ۱/۰ - آشنایی با pintos
======================

شماره گروه:
-----
> نام و آدرس پست الکترونیکی اعضای گروه را در این قسمت بنویسید.

حمیدرضا کلباسی <hamidrezakalbasi@protonmail.com>

مهدی جعفری <mahdi.jfri.79@gmail.com>

میکائیل قربانی <mikaeelghr@gmail.com>

یاسین نوران <ynooran@gmail.com>

مقدمات
----------
> اگر نکات اضافه‌ای در مورد تمرین یا برای دستیاران آموزشی دارید در این قسمت بنویسید.


> لطفا در این قسمت تمامی منابعی (غیر از مستندات Pintos، اسلاید‌ها و دیگر منابع  درس) را که برای تمرین از آن‌ها استفاده کرده‌اید در این قسمت بنویسید.

آشنایی با pintos
============
>  در مستند تمرین گروهی ۱۹ سوال مطرح شده است. پاسخ آن ها را در زیر بنویسید.


## یافتن دستور معیوب

۱. 0xc0000008

۲. 0x8048757

۳. _start

۴. 
کامپایلر در ابتدا فضای لازم را با کم کردن از استک پوینتر فراهم می کند
8048754:       83 ec 1c                sub    $0x1c,%esp
 سپس مقادیر ورودی تابع مین را در جایگاه مناسب قرار می دهد
 8048757:       8b 44 24 24             mov    0x24(%esp),%eax
 804875b:       89 44 24 04             mov    %eax,0x4(%esp)
 804875f:       8b 44 24 20             mov    0x20(%esp),%eax
 8048763:       89 04 24                mov    %eax,(%esp)
 و تابع مین را صدا می زند
 8048766:       e8 35 f9 ff ff          call   80480a0 <main>
       و در ادامه به همین ترتیب تابع اکزیت را با خروجی تابع مین صدا می زند.
 804876b:       89 04 24                mov    %eax,(%esp)
 804876e:       e8 49 1b 00 00          call   804a2bc <exit>

۵.
  می خواست ورودی های تابع را در استک قرار دهد تا آماده صدا کردن تابع مین شود

## به سوی crash

۶.
    نام ریسه فعلی: main
    آدرس ریسه فعلی: 
                    0xc000e000

    ترد فعلی:
    pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000edec <incomplete sequence \35
    7>, priority = 31, allelem = {prev = 0xc0035910 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0035920 <ready_list>, next = 0xc0035928 <ready_list+8>}, pag
    edir = 0x0, magic = 3446325067}
    ترد های دیگر:
    pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allele
    m = {prev = 0xc000e020, next = 0xc0035918 <all_list+8>}, elem = {prev = 0xc0035920 <ready_list>, next = 0xc0035928 <ready_list+8>}, pagedir = 0x0, magic = 344
    6325067}
۷.
    #0  process_execute (file_name=file_name@entry=0xc0007d50 "do-nothing") at ../../userprog/process.c:32
    #1  0xc0020268 in run_task (argv=0xc00357cc <argv+12>) at ../../threads/init.c:288
    #2  0xc0020921 in run_actions (argv=0xc00357cc <argv+12>) at ../../threads/init.c:340
    #3  main () at ../../threads/init.c:133

    کد های سی:

        printf ("Executing '%s':\n", task);
    287│ #ifdef USERPROG
    288├>  process_wait (process_execute (task));
    289│ #else
    290│   run_test (task);

    338│
    339│       /* Invoke action and advance. */
    340├>      a->function (argv);
    341│       argv += a->argc;



۸.

۹.

۱۰.

۱۱.

۱۲.

۱۳.


## دیباگ

۱۴.

۱۵.

۱۶.

۱۷.

۱۸.

۱۹.