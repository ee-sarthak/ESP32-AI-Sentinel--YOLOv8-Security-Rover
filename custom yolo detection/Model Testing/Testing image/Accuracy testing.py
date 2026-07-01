from ultralytics import YOLO

model = YOLO(
    r"C:\Users\SARTHAK\OneDrive\Documents\SELF PROJECTS\custom yolo object detection\Model\best.pt"
)

metrics = model.val(
    data=r"C:\Users\SARTHAK\OneDrive\Documents\SELF PROJECTS\custom yolo object detection\Dataset\dataset.v1-pdcbb.yolov8\data.yaml"
)

print("\n--- MODEL ACCURACY ---")
print("Precision:", metrics.box.mp)
print("Recall:", metrics.box.mr)
print("mAP50:", metrics.box.map50)
print("mAP50-95:", metrics.box.map) 










#output:
"""
--- MODEL ACCURACY ---
Precision: 0.35391713093200144
Recall: 0.26255105928343403
mAP50: 0.2633424811537229
mAP50-95: 0.1516259692835987 
"""