from ultralytics import YOLO
import cv2
import os

model = YOLO(r"C:\Users\SARTHAK\OneDrive\Documents\SELF PROJECTS\custom yolo object detection\Model\best.pt")


def real_time_detection():
    print("Starting Camera Detection... Press q to exit")

    cap = cv2.VideoCapture(0)

    while True:
        ret, frame = cap.read()

        if not ret:
            break

        results = model(frame)
        annotated_frame = results[0].plot()

        cv2.imshow("YOLO Real Time Detection", annotated_frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()



def video_detection():

    path = input("Enter video path: ")

    cap = cv2.VideoCapture(path)

    print("Video Detection Started... Press q to exit")

    while True:
        ret, frame = cap.read()

        if not ret:
            break

        results = model(frame)
        annotated_frame = results[0].plot()

        cv2.imshow("YOLO Video Detection", annotated_frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()



def image_detection():

    path = input("Enter image or folder path: ")

    results = model.predict(
        source=path,
        stream=True   # important for multiple images
    )

    print("Press q for next image")

    for result in results:

        img = result.plot()

        cv2.imshow("YOLO Image Detection", img)

        while True:
            key = cv2.waitKey(100) & 0xFF

            if key == ord('q'):
                break

            elif key == ord('x'):   # press x to exit completely
                cv2.destroyAllWindows()
                return


    cv2.destroyAllWindows()



print("""
===========================
 YOLO OBJECT DETECTION
===========================

1. Real Time Camera
2. Video Detection
3. Image Detection

===========================
""")


choice = input("Choose option (1/2/3): ")


if choice == "1":

    real_time_detection()


elif choice == "2":

    video_detection()


elif choice == "3":

    image_detection()


else:

    print("Invalid Choice")


print("Done")