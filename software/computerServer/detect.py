import argparse
import os
import sys
import requests
from pathlib import Path
import pathlib
import torch
from collections import Counter
from phevaluator.evaluator import evaluate_cards

# --- USER SETTINGS ---
my_chips = 1500    
current_bet = 100  
ESP32_IP = "192.168.137.110"  
# ---------------------

detection_buffer = [] 
REQUIRED_FRAMES = 3 
full_deck_seen = set()
running_count = 0
last_sent_action = "" 

pathlib.PosixPath = pathlib.WindowsPath
FILE = Path(__file__).resolve()
ROOT = FILE.parents[0]
if str(ROOT) not in sys.path:
    sys.path.append(str(ROOT))

from ultralytics.utils.plotting import Annotator, colors
from models.common import DetectMultiBackend
from utils.dataloaders import IMG_FORMATS, VID_FORMATS, LoadImages, LoadStreams
from utils.general import (check_img_size, check_imshow, cv2, 
                           non_max_suppression, scale_boxes)
from utils.torch_utils import select_device, smart_inference_mode

def send_action_to_esp(action):
    """Maps strategy strings to a single clean word for the ESP32."""
    global last_sent_action
    
    # 1. Simplify strategy to a single word
    action_upper = action.upper()
    if "RAISE" in action_upper or "ALL-IN" in action_upper:
        simple_action = "RAISE"
    elif "CALL" in action_upper:
        simple_action = "CALL"
    elif "FOLD" in action_upper:
        simple_action = "FOLD"
    elif "CHECK" in action_upper:
        simple_action = "CHECK"
    else:
        simple_action = "WAIT"

    # 2. Only send if the action has changed to save bandwidth/latency
    if simple_action == last_sent_action:
        return
    
    try:
        url = f"http://{ESP32_IP}/action?val={simple_action}"
        requests.get(url, timeout=0.1) 
        last_sent_action = simple_action
    except Exception:
        pass

def check_esp_button():
    global current_bet, user_hand, detection_buffer
    try:
        url = f"http://{ESP32_IP}/getButton"
        response = requests.get(url, timeout=0.1)
        text = response.text.strip()

        if text == "PRESSED":
            current_bet += 100
            print(f"\n[EVENT] Button Pressed! New Bet: ${current_bet}")
        elif text == "RESET":
            current_bet = 100 # Default starting bet
            user_hand.clear()
            detection_buffer.clear()
            print(f"\n[EVENT] RESET! Bet reset, hand cleared.")
    except Exception:
        pass

def update_card_count(card_id):
    global running_count
    val = card_id[:-1]
    if val in ['2', '3', '4', '5', '6']:
        running_count += 1
    elif val in ['10', 'J', 'Q', 'K', 'A']:
        running_count -= 1

def get_dynamic_strategy(hand, chips, bet):
    count = len(hand)
    if count == 0: return "WAITING"
    fmt_hand = [c.replace('10', 'T') if '10' in c else c for c in hand]
    val_counts = Counter([c[0] for c in fmt_hand])
    risk_ratio = bet / chips if chips > 0 else 1.0
    is_massive_bet = risk_ratio >= 0.9

    if any(n >= 3 for n in val_counts.values()):
        return "TRIPS - ALL-IN / CALL"

    pairs = [v for v, n in val_counts.items() if n == 2]
    if len(pairs) >= 1:
        if is_massive_bet:
            return "HIGH PAIR - CALL" if any(p in ['A', 'K', 'Q', 'J'] for p in pairs) else "FOLD"
        return "PAIR - RAISE"

    if count < 5:
        if any(v in val_counts for v in ['A', 'K']):
            return "HIGH CARD - CALL" if not is_massive_bet else "FOLD"
        return "FOLD"

    try:
        eval_hand = fmt_hand[:5] if count < 7 else fmt_hand[:7]
        rank = evaluate_cards(*eval_hand)
        if rank < 1600: return "MONSTER - RAISE" 
        if rank < 3500: return "STRONG - CALL"
        return "FOLD"
    except:
        return "EVAL ERROR"

@smart_inference_mode()
def run(
        weights='poker.pt',
        source='0',
        data='data/coco128.yaml',
        imgsz=(640, 640),
        conf_thres=0.4,
        iou_thres=0.45,
        max_det=1000,
        device='',
        view_img=True,
        line_thickness=3,
        vid_stride=1,
):
    global detection_buffer, running_count, current_bet
    user_hand = []
    
    device = select_device(device)
    model = DetectMultiBackend(weights, device=device, fp16=False)
    stride, names = model.stride, model.names
    imgsz = check_img_size(imgsz, s=stride)

    dataset = LoadStreams(source, img_size=imgsz, stride=stride, auto=True, vid_stride=vid_stride) if source.isnumeric() or source.startswith('http') else LoadImages(source, img_size=imgsz, stride=stride, auto=True, vid_stride=vid_stride)

    for path, im, im0s, vid_cap, s in dataset:
        im = torch.from_numpy(im).to(model.device).float() / 255
        if len(im.shape) == 3: im = im[None]
        pred = model(im)
        pred = non_max_suppression(pred, conf_thres, iou_thres, max_det=max_det)
        
        # Poll ESP button status every frame
        check_esp_button()
        
        for i, det in enumerate(pred):
            im0 = im0s[i].copy() if isinstance(dataset, LoadStreams) else im0s.copy()

            if len(det):
                det[:, :4] = scale_boxes(im.shape[2:], det[:, :4], im0.shape).round()
                for *xyxy, conf, cls in reversed(det):
                    card_id = names[int(cls)]
                    detection_buffer.append(card_id)
                    if detection_buffer.count(card_id) >= REQUIRED_FRAMES:
                        if card_id not in user_hand:
                            user_hand.append(card_id)
                        if card_id not in full_deck_seen:
                            full_deck_seen.add(card_id)
                            update_card_count(card_id)

            if len(detection_buffer) > 30: detection_buffer = detection_buffer[-30:]

            advice = get_dynamic_strategy(user_hand, my_chips, current_bet)
            send_action_to_esp(advice)

            # Clean CLI output
            sys.stdout.write(f"\rBET: ${current_bet} | HAND: {user_hand} | COUNT: {running_count} | ACTION: {advice}    ")
            sys.stdout.flush()

            if view_img:
                cv2.rectangle(im0, (5, 5), (600, 150), (0,0,0), -1)
                cv2.putText(im0, f"BET: ${current_bet}", (20, 30), 0, 0.6, (200, 200, 200), 2)
                cv2.putText(im0, f"HAND: {user_hand}", (20, 70), 0, 0.7, (255, 255, 255), 2)
                act_color = (0, 255, 0) if "CALL" in advice or "RAISE" in advice else (0, 0, 255)
                cv2.putText(im0, f"ACTION: {advice}", (20, 120), 0, 1.2, act_color, 3)
                cv2.imshow("Omi Smart Poker", im0)
                if cv2.waitKey(1) == ord('r'): 
                    user_hand = []
                    detection_buffer = []
                    current_bet = 100 

def parse_opt():
    parser = argparse.ArgumentParser()
    parser.add_argument('--weights', type=str, default='poker.pt')
    parser.add_argument('--source', type=str, default='0')
    parser.add_argument('--data', type=str, default='data/coco128.yaml')
    parser.add_argument('--imgsz', '--img', nargs='+', type=int, default=[640])
    parser.add_argument('--conf-thres', type=float, default=0.4)
    parser.add_argument('--iou-thres', type=float, default=0.45)
    parser.add_argument('--max-det', type=int, default=1000)
    parser.add_argument('--device', default='')
    parser.add_argument('--view-img', action='store_true')
    parser.add_argument('--line-thickness', default=3, type=int)
    parser.add_argument('--vid-stride', type=int, default=1)
    opt = parser.parse_args()
    opt.imgsz *= 2 if len(opt.imgsz) == 1 else 1
    return opt

def main(opt):
    run(**vars(opt))

if __name__ == "__main__":
    opt = parse_opt()
    main(opt)