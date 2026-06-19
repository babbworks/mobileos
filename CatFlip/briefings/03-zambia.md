# Babb OS — Zambia Deployment Context
*Three-page briefing for market and product readers*

---

## Why Zambia

Zambia is not an afterthought or a test market. It is the design center. Every technical decision — network bands, audio codec, power management, default language, emergency numbers, APNs, mobile money integration — has been made with Zambia as the primary deployment environment. The Cat S22 Flip's network radio supports all active LTE bands in use by all three Zambian mobile operators. This is not coincidence; it is the starting condition that made this particular device worth choosing.

Zambia also presents a set of constraints that make the custom OS case stronger than in wealthier markets. Electricity access is irregular outside urban centers, making battery life a genuine daily concern. Mobile data is expensive relative to income, making background data consumption a real cost to the user. Banking infrastructure is limited, making mobile money platforms like Airtel Money, Zamtel Kwacha, and MTN MoMo the primary financial tools for much of the population. A phone that drains its battery running Google services in the background, sends data to US cloud endpoints, and fails to support mobile money menus correctly is simply a worse phone for this context.

---

## The Three Carriers

**Airtel Zambia** is the largest carrier by subscriber count and the most technically mature in terms of LTE deployment. They operate on LTE Bands 3 (1800MHz), 7 (2600MHz), and 8 (900MHz). Band 8 is particularly important — lower frequency signals travel farther and penetrate buildings better, which matters in rural and peri-urban Zambia. Airtel Money (*778#) is accessible via both STK SIM menu and USSD dialing. Airtel has deployed VoLTE on their LTE network, making them the most promising carrier for high-quality voice calls on Babb OS.

**Zamtel** is the state-owned operator. Their LTE deployment uses Bands 7 (2600MHz) and 40 (2300MHz TDD). Their WCDMA (3G) network uses Band 1. Zamtel Kwacha Money (*303#) provides mobile financial services. Zamtel coverage is stronger in some rural areas where Airtel has less presence, making both-carrier compatibility important for a device intended for broad Zambian deployment.

**MTN Zambia** operates LTE on Band 3 (1800MHz) with WCDMA on Band 1. MTN MoMo (*112#) is their mobile money platform. MTN's Zambian network is less extensive than in other African markets but serves an important segment, particularly in certain urban and commercial areas.

All three carriers fall within the QM215 modem's supported band set. No hardware modifications, no RF recertification. The device works on all three networks out of the box.

---

## Mobile Money as Core Infrastructure

This deserves emphasis: mobile money is not a convenience feature in Zambia. It is how people pay school fees, receive wages, buy goods at the market, and transfer funds to family in different provinces. The proportion of Zambians with mobile money accounts vastly exceeds those with bank accounts.

Mobile money access works through two parallel mechanisms on Android:

**STK (SIM Toolkit):** The SIM card itself runs a small application that presents menus on the phone's screen. This requires no internet connection, no data, and no backend server — it runs entirely over the GSM signaling channel. When a user inserts an Airtel SIM, a menu entry appears in the phone app or notifications giving access to Airtel Money features. If `com.android.stk` is not installed or is killed by battery optimization, this menu disappears. Babb OS keeps this application and exempts it from all battery restrictions.

**USSD (Unstructured Supplementary Service Data):** Short codes dialed as phone numbers (`*778#`, `*303#`, `*112#`) open interactive text sessions over the network's signaling channel. These work on 2G — no LTE or data plan required. The AOSP dialer handles USSD natively. Every Babb OS release is tested for USSD functionality on each carrier before distribution.

The failure mode to avoid: a custom ROM that removes STK as "bloatware" or kills it with aggressive battery management. We have documented this explicitly and built the system to prevent it.

---

## Language and Localization

Zambia is an English-medium country for official and commercial purposes, but the national languages — Bemba, Nyanja (Chichewa), Tonga, and Lozi — are the languages of daily life. Babb OS ships with:

- Default locale: `en_ZM` (English, Zambia)
- Default timezone: `Africa/Lusaka` (UTC+2, no daylight saving)
- Additional locale support compiled in: Bemba (`bem`), Nyanja (`ny`), Tonga (`tog`), Lozi (`loz`)
- Keyboard layouts: QWERTY English + regional character support for local language extended characters
- Offline maps (Organic Maps) pre-seeded with Zambia data

The stock Android setup wizard is replaced with a Babb-specific first-run experience that does not contact Google, does not ask for a Google account, and prompts for locale and language selection from the Zambia-relevant set.

---

## Power and Connectivity in Context

The 1450mAh battery is modest. In a market context where a user may go 24–48 hours between reliable charging opportunities, "good battery life" is not a comfort feature — it is a reliability feature.

Babb OS targets a deep idle power floor below 50mW and an LTE standby envelope below 100mW. The practical implication: a fully charged device left idle on an LTE Airtel network should have meaningful battery remaining after 24 hours of standby. This is achievable because GMS services (which run continuously and prevent the modem from entering deep sleep) are absent, and because the modem is configured for extended paging intervals (eDRX).

WiFi is common in urban Zambia — offices, cafes, and homes increasingly have broadband. WiFi power save mode is enabled by default. When on WiFi, mobile data dormancy is automatic.

The device's physical durability (IP68 rating, reinforced corners) is also relevant in field conditions — it is designed for environments where it will be dropped and exposed to dust and moisture, which aligns with actual usage patterns in Zambia.

---

## What This Enables

A Zambian user with a Babb OS device has:

- A phone that works with any of the three Zambian carriers without configuration
- Full access to Airtel Money, Zamtel Kwacha Money, and MTN MoMo via USSD and STK
- No ongoing cost from background data sent to Google or other cloud services
- A default language and timezone that match their environment
- Emergency numbers pre-configured for Zambian emergency services
- Battery life optimized for irregular charging
- Offline maps of Zambia without requiring a data plan
- A software platform that is openly maintainable and can be updated independently of any commercial app store or cloud provider

This is the gap the project fills: a phone experience built for this context, not adapted from one built for a different one.

---

*For the full technical and strategic picture including development roadmap and risks, see `05-full-brief.md`.*
